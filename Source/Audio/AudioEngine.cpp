#include "AudioEngine.h"
#include "AudioRecorder.h"
#include "RegionAudio.h"
#include "Session/AutomationModel.h"
#include "Midi/MidiIO.h"
#include "Midi/MidiRuntime.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace nova {
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<double>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);

std::unique_ptr<Clip> AudioEngine::loadWav(const juce::File& file, juce::String& error) {
    if (!file.hasFileExtension("wav")) { error = "Importe um arquivo WAV mono ou estereo."; return {}; }
    juce::WavAudioFormat format;
    auto stream = file.createInputStream();
    if (!stream) { error = "Nao foi possivel abrir: " + file.getFullPathName(); return {}; }
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(), true));
    if (!reader || reader->lengthInSamples <= 0 || reader->sampleRate <= 0
        || !std::isfinite(reader->sampleRate) || reader->numChannels < 1 || reader->numChannels > 2) {
        error = "WAV invalido ou formato nao suportado (somente mono/estereo): " + file.getFileName(); return {};
    }
    if (reader->lengthInSamples > std::numeric_limits<int>::max()
        || reader->lengthInSamples * static_cast<int64_t>(reader->numChannels) * sizeof(float) > maxSessionBytes) {
        error = "O arquivo excede o limite de 512 MiB de audio decodificado."; return {};
    }
    auto clip = std::make_unique<Clip>();
    clip->source = file; clip->name = file.getFileNameWithoutExtension(); clip->sampleRate = reader->sampleRate;
    clip->samples.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&clip->samples, 0, clip->samples.getNumSamples(), 0, true, true)) {
        error = "Erro ao decodificar WAV: " + file.getFileName(); return {};
    }
    const int bins = std::min(4096, clip->samples.getNumSamples());
    clip->waveform.resize(static_cast<size_t>(bins));
    for (int i = 0; i < bins; ++i) {
        const int start = static_cast<int>(static_cast<int64_t>(i) * clip->samples.getNumSamples() / bins);
        const int end = static_cast<int>(static_cast<int64_t>(i + 1) * clip->samples.getNumSamples() / bins);
        Peak peak;
        for (int channel = 0; channel < clip->samples.getNumChannels(); ++channel) {
            auto* data = clip->samples.getReadPointer(channel);
            for (int sample = start; sample < end; ++sample) {
                if (!std::isfinite(data[sample])) { error = "WAV contem amostras nao finitas."; return {}; }
                peak.low = std::min(peak.low, data[sample]); peak.high = std::max(peak.high, data[sample]);
            }
        }
        clip->waveform[static_cast<size_t>(i)] = peak;
    }
    return clip;
}

bool AudioEngine::addClip(std::unique_ptr<Clip> clip, juce::String& error) {
    return addSharedClip(std::shared_ptr<Clip>(std::move(clip)),error);
}
void AudioEngine::recalculateMediaBytes() {
    MutationGuard mutation(*this);
    std::set<const Clip*> assets;
    for(int i=0;i<count;++i) { assets.insert(track(i).clip.get()); for(const auto& r:track(i).regions){assets.insert(r.asset.get());assets.insert(r.original.get());} }
    bytes=0;
    for(const auto* asset:assets) if(asset) bytes+=static_cast<int64_t>(asset->samples.getNumSamples())*asset->samples.getNumChannels()*sizeof(float);
}
bool AudioEngine::addSharedClip(std::shared_ptr<Clip> clip, juce::String& error) {
    MutationGuard mutation(*this);
    if (!clip) { error = "Clip vazio."; return false; }
    bool exists=false;
    for(int i=0;i<count;++i) { exists|=track(i).clip.get()==clip.get(); for(const auto& r:track(i).regions)exists|=r.asset.get()==clip.get(); }
    const int64_t clipBytes = exists ? 0 : static_cast<int64_t>(clip->samples.getNumChannels()) * clip->samples.getNumSamples() * sizeof(float);
    if (count == maxTracks || bytes + clipBytes > maxSessionBytes) {
        error = "Limite: 64 faixas e 512 MiB de audio decodificado por sessao."; return false;
    }
    auto& t = tracks[static_cast<size_t>(count++)];
    t.vcaGroup=t.channelGroup=t.sidechainSource=-1;t.soloSafe=false;t.recordArm=false;t.monitorInput=false;t.monitorPreference=false;t.recordGain=1;t.inputPeak=0;t.recordInputLeft=0;t.recordInputRight=1;t.recordStereo=false;t.takes.clear();t.activeTake=0;t.auxiliary=false;t.midiTrack=false;t.midiClips.clear();t.midiEvents.clear();t.instrument.reset();t.midiInput.clear();t.midiArm=false;t.monoBus=false;t.outputTarget=-1;t.inputGain=1;t.rms=0;t.clipped=false;t.automation.clear();t.automationOverrideMask=0;t.automationMode=AutomationMode::off;
    for(auto& p:t.inserts)p.reset();for(auto& send:t.sends){send.target=-1;send.gain=0;send.pan=0;send.width=1;send.mute=false;send.preFader=false;}
    t.clip = std::move(clip); t.gain = 1; t.pan = 0; t.mute = false; t.solo = false; t.peak = 0;
    t.name = t.clip->name; t.height = 140; t.outputMode = 0;
    t.colour = juce::Colour::fromHSV(std::fmod(0.47f + (count - 1) * 0.13f, 1.0f), 0.5f, 0.88f, 1.0f).getARGB();
    t.regions = {{newRegionId(), t.clip, 0, 0, t.clip->duration()}};
    bytes += clipBytes; processingOrder[count-1]=count-1;smoothingInitialized=false;
    if(!rebuildGraph(error)){--count;bytes-=clipBytes;t.regions.clear();t.clip.reset();return false;}return true;
}
void AudioEngine::clear() {
    MutationGuard mutation(*this);for(auto& route:midiIO->routes)route=0;++midiIO->generation;midiRecording=false;if(automation)automation->reset();
    stop();stopPreview();masteringEnabled=false;mastering->reset();stemSource=-1;exportOptions={};recordingExtent=0;audioRecording=false;
    for (auto& t : tracks) { t.regions.clear(); t.clip.reset();for(auto& p:t.inserts)p.reset();t.instrument.reset();t.midiClips.clear();t.midiEvents.clear();t.midiTrack=false;t.midiArm=false;t.vcaGroup=t.channelGroup=t.sidechainSource=-1;t.soloSafe=false;t.recordArm=false;t.monitorInput=false;t.takes.clear();t.activeTake=0;t.automation.clear();t.automationOverrideMask=0; }
    for(auto& p:masterInserts)p.reset();masterMute=false;masterClipped=false;masterPeak=masterRms=0;smoothingInitialized=false;
    count = 0; bytes = 0; recordingOptions={};masterGain = 1;compensation={};visibleLatency=0;pdcReady=true;configurationError.clear();
    for(int i=0;i<8;++i){vcaGroups[i].gain=1;vcaGroups[i].mute=false;vcaGroups[i].solo=false;vcaGroups[i].name="VCA "+juce::String(i+1);channelGroups[i].gain=1;channelGroups[i].mute=false;channelGroups[i].solo=false;channelGroups[i].name="Grupo "+juce::String(i+1);}loopEnabled = false; loopStart = 0; loopEnd = 0; bpm = 120; beatsPerBar = beatUnit = 4;
}
double AudioEngine::duration() const {
    double end = 0;
    for (int i = 0; i < count; ++i) {for (const auto& region : tracks[static_cast<size_t>(i)].regions) end = std::max(end, region.end());
        for(const auto& clip:track(i).midiClips)end=std::max(end,ticksToSeconds(clip.start+clip.length,bpm.load()));}
    return std::max(end,recordingExtent.load());
}
void AudioEngine::setLoop(double start, double end, bool enabled) {
    MutationGuard mutation(*this);
    if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end <= start || end > duration() + 1e-8) {
        loopEnabled = false; return;
    }
    loopStart = start; loopEnd = end; loopEnabled = enabled;
}
void AudioEngine::seek(double seconds) {
    MutationGuard mutation(*this);
    if (!std::isfinite(seconds)) return;
    midiRecording=false;audioRecording=false;midiIO->pendingCount=0;LiveMidiEvent discarded;for(int i=0;i<2048&&midiIO->incoming.tryPop(discarded);++i){}
    const double value = std::clamp(seconds, 0.0, duration());
    for(int i=0;i<count;++i){if(track(i).instrument)track(i).instrument->reset();for(auto& insert:track(i).inserts)if(insert)insert->reset();}
    for(auto& insert:masterInserts)if(insert)insert->reset();
    requestedPosition.store(value, std::memory_order_relaxed);
    visiblePosition.store(value);
    seekGeneration.fetch_add(1, std::memory_order_release);
}
AudioEngine::MutationGuard::MutationGuard(AudioEngine& e):engine(e){
    if(engine.mutationDepth++==0){engine.processingGate.fetch_or(2,std::memory_order_acq_rel);
        while((engine.processingGate.load(std::memory_order_acquire)&1)!=0)std::this_thread::yield();}
}
AudioEngine::MutationGuard::~MutationGuard(){if(--engine.mutationDepth==0)engine.processingGate.fetch_and(~2u,std::memory_order_release);}
AudioEngine::~AudioEngine()=default;
void AudioEngine::finishAutomation(){automation->finishAll();}
void AudioEngine::pause(){previewPlaying=false;playing=false;midiRecording=false;audioRecording=false;midiIO->panic=true;}
AudioEngine::AudioEngine() {
    for(int i=0;i<8;++i){vcaGroups[i].name="VCA "+juce::String(i+1);channelGroups[i].name="Grupo "+juce::String(i+1);}
    extended=std::make_unique<MixExtensions>();automation=std::make_unique<AutomationModel>(*this);mastering=std::make_unique<MasteringMeter>();
    midiIO=std::make_unique<MidiIO>();
    for(auto& buffer:channelBuffers)buffer.setSize(2,mixBlockSize);for(auto& buffer:extended->detectorTaps)buffer.setSize(2,mixBlockSize);extended->detectorBuffer.setSize(2,mixBlockSize);for(auto& buffer:extended->detectorStages)buffer.setSize(2,mixBlockSize);
    masterBuffer.setSize(2,mixBlockSize);for(auto& buffer:scheduledMidi)buffer.ensureSize(8192);midiSpan.ensureSize(8192);lastMidiTime.fill(-1);setSmoothingTime(smoothingTime);
}
void AudioEngine::setSmoothingTime(double seconds) {
    MutationGuard mutation(*this);
    smoothingTime=std::clamp(seconds,0.0,0.1);smoothingInitialized=false;
    for(int i=0;i<maxTracks;++i){smoothGain[i].reset(outputRate,smoothingTime);smoothPan[i].reset(outputRate,smoothingTime);smoothInput[i].reset(outputRate,smoothingTime);extended->smoothGroupGain[i].reset(outputRate,smoothingTime);smoothAudible[i].reset(outputRate,smoothingTime);for(auto& send:smoothSend[i])send.reset(outputRate,smoothingTime);for(auto& send:extended->smoothSendPan[i])send.reset(outputRate,smoothingTime);for(auto& send:extended->smoothSendWidth[i])send.reset(outputRate,smoothingTime);}
    smoothMaster.reset(outputRate,smoothingTime);smoothMasterAudible.reset(outputRate,smoothingTime);
}
void AudioEngine::setOutputRate(double rate) {
    MutationGuard mutation(*this);
    // Preserve the unwrapped automation clock when its sample unit changes.
    seekOrigin+=elapsedSinceSeek/outputRate.load();elapsedSinceSeek=0;
    outputRate=std::isfinite(rate)&&rate>=100?rate:44100;requestedDeviceRate=outputRate.load();setSmoothingTime(smoothingTime);prepareProcessors();
}
void AudioEngine::prepareProcessors() {
    MutationGuard mutation(*this);
    try{
        mastering->prepare(outputRate.load());
        playheadBeats=beatsPerBar;playheadUnit=beatUnit;
        for(int i=0;i<count;++i){if(auto& instrument=track(i).instrument){instrument->prepare(outputRate);if(instrument->plugin)instrument->plugin->setPlayHead(this);}
            for(auto& insert:track(i).inserts)if(insert){insert->prepare(outputRate);if(insert->plugin)insert->plugin->setPlayHead(this);}}
        for(auto& insert:masterInserts)if(insert){insert->prepare(outputRate);if(insert->plugin)insert->plugin->setPlayHead(this);}
        juce::String error;if(!rebuildGraph(error)){configurationError=error;pdcReady=false;}
    }catch(const std::exception& ex){configurationError="Falha ao preparar processamento: "+juce::String(ex.what());pdcReady=false;}
}
bool AudioEngine::needsMaintenance()const {
    if(requestedDeviceRate.load()!=outputRate.load())return true;
    for(int i=0;i<count;++i){if(track(i).instrument&&track(i).instrument->latencyChanged())return true;for(const auto& insert:track(i).inserts)if(insert && insert->latencyChanged())return true;}
    for(const auto& insert:masterInserts)if(insert && insert->latencyChanged())return true;return false;
}
uint64_t AudioEngine::contentionDropouts()const {
    uint64_t total=0;for(int i=0;i<count;++i){if(track(i).instrument)total+=track(i).instrument->contentionDropouts();for(const auto& insert:track(i).inserts)if(insert)total+=insert->contentionDropouts();}
    for(const auto& insert:masterInserts)if(insert)total+=insert->contentionDropouts();return total;
}
size_t AudioEngine::ownedBufferBytes()const {
    size_t total=mastering->ownedBytes()+size_t(bytes)+compensation.allocatedBytes+clickCompensation.bytes();
    for(const auto& buffer:channelBuffers)total+=size_t(buffer.getNumSamples())*buffer.getNumChannels()*sizeof(float);
    total+=size_t(masterBuffer.getNumSamples())*masterBuffer.getNumChannels()*sizeof(float);for(const auto& buffer:extended->detectorTaps)total+=size_t(buffer.getNumSamples())*buffer.getNumChannels()*sizeof(float);for(const auto& buffer:extended->detectorStages)total+=size_t(buffer.getNumSamples())*buffer.getNumChannels()*sizeof(float);total+=size_t(extended->detectorBuffer.getNumSamples())*extended->detectorBuffer.getNumChannels()*sizeof(float);
    for(int i=0;i<count;++i){if(track(i).instrument)total+=track(i).instrument->ownedBufferBytes();for(const auto& p:track(i).inserts)if(p)total+=p->ownedBufferBytes();}
    for(const auto& p:masterInserts)if(p)total+=p->ownedBufferBytes();return total;
}
void AudioEngine::closeEditors(){MutationGuard mutation(*this);for(int i=0;i<count;++i){if(track(i).instrument)track(i).instrument->closeEditor();for(auto& p:track(i).inserts)if(p)p->closeEditor();}for(auto& p:masterInserts)if(p)p->closeEditor();}
void AudioEngine::replaceSession(AudioEngine& other) {
    MutationGuard mutation(*this);
    clear();
    for(int i=0;i<other.count;++i){auto& src=other.track(i);auto& dst=track(i);
        dst.vcaGroup=src.vcaGroup;dst.channelGroup=src.channelGroup;dst.sidechainSource=src.sidechainSource;dst.soloSafe=src.soloSafe.load();dst.clip=std::move(src.clip);dst.regions=std::move(src.regions);dst.name=src.name;dst.colour=src.colour;dst.height=src.height;
        dst.gain=src.gain.load();dst.pan=src.pan.load();dst.mute=src.mute.load();dst.solo=src.solo.load();dst.outputMode=src.outputMode.load();
        dst.inputGain=src.inputGain.load();dst.outputTarget=src.outputTarget;dst.auxiliary=src.auxiliary;dst.monoBus=src.monoBus;
        dst.midiTrack=src.midiTrack;dst.midiClips=std::move(src.midiClips);dst.midiEvents=std::move(src.midiEvents);dst.instrument=std::move(src.instrument);dst.midiInput=src.midiInput;dst.midiArm=src.midiArm.load();dst.recordArm=src.recordArm.load();dst.monitorInput=false;dst.monitorPreference=src.monitorPreference;dst.recordGain=src.recordGain.load();dst.recordInputLeft=src.recordInputLeft;dst.recordInputRight=src.recordInputRight;dst.recordStereo=src.recordStereo;dst.takes=std::move(src.takes);dst.activeTake=src.activeTake;
        dst.peak=dst.rms=0;dst.clipped=false;dst.inserts=std::move(src.inserts);dst.automation=std::move(src.automation);dst.automationMode=src.automationMode;
        for(int j=0;j<2;++j){dst.sends[j].pan=src.sends[j].pan.load();dst.sends[j].width=src.sends[j].width.load();dst.sends[j].mute=src.sends[j].mute.load();dst.sends[j].target=src.sends[j].target;dst.sends[j].preFader=src.sends[j].preFader;dst.sends[j].gain=src.sends[j].gain.load();}
    }
    for(int i=0;i<8;++i){vcaGroups[i].name=other.vcaGroups[i].name;vcaGroups[i].gain=other.vcaGroups[i].gain.load();vcaGroups[i].mute=other.vcaGroups[i].mute.load();channelGroups[i].name=other.channelGroups[i].name;channelGroups[i].gain=other.channelGroups[i].gain.load();channelGroups[i].mute=other.channelGroups[i].mute.load();channelGroups[i].solo=other.channelGroups[i].solo.load();}exportOptions=other.exportOptions;count=other.count;bytes=other.bytes;nextRegionId=other.nextRegionId;processingOrder=other.processingOrder;
    masterGain=other.masterGain.load();masterMute=other.masterMute.load();masterInserts=std::move(other.masterInserts);
    compensation=std::move(other.compensation);visibleLatency=compensation.totalLatency;pdcReady=other.pdcReady.load();configurationError=other.configurationError;
    recordingOptions=other.recordingOptions;bpm=other.bpm.load();beatsPerBar=other.beatsPerBar;beatUnit=other.beatUnit;
    loopStart=other.loopStart.load();loopEnd=other.loopEnd.load();loopEnabled=other.loopEnabled.load();
    metronomeEnabled=other.metronomeEnabled.load();resumeMidi.fill(true);lastMidiTime.fill(-1);synchronizeMidiInputs();prepareProcessors();
}
bool AudioEngine::addAux(const juce::String& name,bool mono,juce::String& error) {
    MutationGuard mutation(*this);
    auto placeholder=std::make_unique<Clip>();placeholder->name=name;placeholder->samples.setSize(2,1);placeholder->samples.clear();
    if(!addClip(std::move(placeholder),error))return false;
    auto& t=track(count-1);t.auxiliary=true;t.monoBus=mono;t.regions.clear();return true;
}
bool AudioEngine::rebuildGraph(juce::String& error) {
    MutationGuard mutation(*this);
    std::vector<PdcNode> nodes;nodes.reserve(static_cast<size_t>(count));
    for(int i=0;i<count;++i){const auto& t=track(i);int latency=t.instrument?t.instrument->latencySamples():0;for(const auto& insert:t.inserts)if(insert)latency+=insert->latencySamples();
        std::array<int,4> insertLatencies{};for(int slot=0;slot<4;++slot)if(t.inserts[slot])insertLatencies[slot]=t.inserts[slot]->latencySamples();nodes.push_back({t.auxiliary,t.outputTarget,latency,{t.sends[0].target,t.sends[1].target},t.sidechainSource,insertLatencies});}
    int masterLatency=0;for(const auto& insert:masterInserts)if(insert)masterLatency+=insert->latencySamples();
    DelayCompensation candidate;if(!candidate.build(nodes,masterLatency,error))return false;
    StereoDelay click;try{click.prepare(candidate.masterInputLatency);}catch(const std::bad_alloc&){error="Memoria insuficiente para metronomo/PDC.";return false;}
    clickCompensation=std::move(click);compensation=std::move(candidate);processingOrder=compensation.order;visibleLatency=compensation.totalLatency;pdcReady=true;configurationError.clear();return true;
}
bool AudioEngine::setRoute(int source,int destination,juce::String& error) {
    MutationGuard mutation(*this);
    if(source<0 || source>=count)return false;auto& t=track(source);const int old=t.outputTarget;t.outputTarget=destination;
    if(rebuildGraph(error))return true;t.outputTarget=old;return false;
}
bool AudioEngine::setSidechain(int index,int source,juce::String& error){MutationGuard guard(*this);if(index<0||index>=count||source<-1||source>=count||source==index||track(index).midiTrack){error="Sidechain exige destino de audio/aux e fonte diferente.";return false;}auto& t=track(index);const int previous=t.sidechainSource;t.sidechainSource=source;if(rebuildGraph(error))return true;t.sidechainSource=previous;return false;}
bool AudioEngine::setSend(int source,int slot,int destination,bool pre,juce::String& error) {
    MutationGuard mutation(*this);
    if(source<0 || source>=count || slot<0 || slot>=2)return false;
    auto& send=track(source).sends[slot];const int old=send.target;const bool oldPre=send.preFader;send.target=destination;send.preFader=pre;
    if(rebuildGraph(error))return true;send.target=old;send.preFader=oldPre;return false;
}
void AudioEngine::render(float* const* output,int channels,int frames) noexcept {
    renderAtTime(output,channels,frames,-1);
}
void AudioEngine::processMasterMeter(float* const* output,int channels,int frames)noexcept{if(!masteringEnabled)return;const float* pointers[]{output[0],channels>1?output[1]:nullptr};mastering->process(pointers,std::min(channels,2),frames);}
void AudioEngine::renderAtTime(float* const* output,int channels,int frames,double monotonicSeconds) noexcept {renderInternal(output,channels,frames,monotonicSeconds,nullptr,0);}
void AudioEngine::renderWithInput(const float* const* input,int inputs,float* const* output,int channels,int frames)noexcept{renderInternal(output,channels,frames,-1,input,inputs);}
void AudioEngine::renderInternal(float* const* output,int channels,int frames,double monotonicSeconds,const float* const* input,int inputs) noexcept {
    if(!output || channels<1 || frames<1)return;
    for(int c=0;c<channels;++c)if(output[c])std::fill_n(output[c],frames,0.0f);
    unsigned expected=0;if(!processingGate.compare_exchange_strong(expected,1,std::memory_order_acquire)){++gatedCallbacks;return;}
    struct Release {std::atomic<unsigned>& gate;~Release(){gate.fetch_sub(1,std::memory_order_release);}} release{processingGate};
    // A rate/latency transition is serviced off-thread; emit silence instead of misaligned audio.
    midiIO->clockSequence.fetch_add(1,std::memory_order_acq_rel);midiIO->renderWallTime=std::isfinite(monotonicSeconds)&&monotonicSeconds>=0?monotonicSeconds:juce::Time::getMillisecondCounterHiRes()*0.001;midiIO->clockFrames=frames;midiIO->clockRate=sampleRate();midiIO->clockSequence.fetch_add(1,std::memory_order_release);
    if(!pdcReady.load() || needsMaintenance()){++maintenanceSilences;return;}
    const auto generation=seekGeneration.load(std::memory_order_acquire);
    if(generation!=consumedSeek){renderPosition=requestedPosition.load(std::memory_order_relaxed);consumedSeek=generation;seekOrigin=renderPosition;elapsedSinceSeek=0;compensation.reset();resumeMidi.fill(true);lastMidiTime.fill(-1);lastClickBeat=-1;clickRemaining=0;clickCompensation.reset();
        // Seeking resets native tails. VST3 reset is a lifecycle operation and stays off the callback.
        for(int i=0;i<count;++i)for(auto& p:track(i).inserts)if(p)p->reset(false);for(auto& p:masterInserts)if(p)p->reset(false);}
    renderInputs=input;renderInputCount=inputs;
    if(recorder&&audioRecording&&playing)recorder->capture(input,inputs,frames,renderPosition);
    for(int i=0;i<count;++i){auto& t=track(i);float peak=0;if(input&&!t.midiTrack&&!t.auxiliary&&t.recordInputLeft>=0&&t.recordInputLeft<inputs&&input[t.recordInputLeft])for(int sample=0;sample<frames;++sample)peak=std::max(peak,std::abs(input[t.recordInputLeft][sample]));if(input&&t.recordStereo&&t.recordInputRight>=0&&t.recordInputRight<inputs&&input[t.recordInputRight])for(int sample=0;sample<frames;++sample)peak=std::max(peak,std::abs(input[t.recordInputRight][sample]));t.inputPeak=peak;}
    consumeMidiInput(frames);bool liveScene=false;for(int i=0;i<count;++i)liveScene|=track(i).midiTrack||(monitoringAllowed&&track(i).monitorInput.load());
    if(!playing&&!liveScene){masterPeak=masterRms=0;for(int i=0;i<count;++i)track(i).peak=track(i).rms=0;renderPreview(output,channels,frames);processMasterMeter(output,channels,frames);return;}
    float masterEnergy=0,masterMaximum=0;std::array<float,maxTracks> channelEnergy{},channelMaximum{};
    for(int offset=0;offset<frames;offset+=mixBlockSize){const int size=std::min(mixBlockSize,frames-offset);renderBlock(output,channels,offset,size);
        masterEnergy+=blockMasterRms*blockMasterRms*size;masterMaximum=std::max(masterMaximum,blockMasterPeak);
        for(int i=0;i<count;++i){channelEnergy[i]+=blockRms[i]*blockRms[i]*size;channelMaximum[i]=std::max(channelMaximum[i],blockPeaks[i]);}
        if(!playing&&!liveScene)break;}
    // Publish meters after the full callback; UI readers never see an intermediate clear.
    masterPeak=masterMaximum;masterRms=std::sqrt(masterEnergy/frames);for(int i=0;i<count;++i){track(i).peak=channelMaximum[i];track(i).rms=std::sqrt(channelEnergy[i]/frames);}
    renderPreview(output,channels,frames);processMasterMeter(output,channels,frames);
    if(midiRecording)midiRecordEndTick=secondsToTicks(renderPosition,bpm.load());
    visiblePosition=midiRecording||audioRecording||(liveScene&&!playing)?renderPosition:std::min(renderPosition,duration());
}
void AudioEngine::startPreview(std::shared_ptr<Clip> clip){MutationGuard guard(*this);pause();previewPlaying=false;previewClip=std::move(clip);previewCursor=0;previewPlaying=bool(previewClip);}
void AudioEngine::stopPreview(){MutationGuard guard(*this);previewPlaying=false;previewClip.reset();previewCursor=0;}
void AudioEngine::renderPreview(float* const* output,int channels,int frames)noexcept{if(!previewPlaying||!previewClip)return;const auto& clip=*previewClip;const double step=clip.sampleRate/sampleRate();float peak=masterPeak;
    for(int n=0;n<frames;++n){if(previewCursor>=clip.samples.getNumSamples()){previewPlaying=false;break;}const int a=int(previewCursor),b=std::min(a+1,clip.samples.getNumSamples()-1);const float fraction=float(previewCursor-a);float values[2]{};for(int c=0;c<2;++c){const int source=std::min(c,clip.samples.getNumChannels()-1);const float* data=clip.samples.getReadPointer(source);values[c]=(data[a]+(data[b]-data[a])*fraction)*0.25f;}if(channels==1){if(output[0])output[0][n]=std::clamp(output[0][n]+(values[0]+values[1])*0.5f,-1.0f,1.0f);}
        else for(int c=0;c<2;++c)if(output[c])output[c][n]=std::clamp(output[c][n]+values[c],-1.0f,1.0f);peak=std::max({peak,std::abs(values[0]),std::abs(values[1])});previewCursor+=step;}masterPeak=peak;
}
void AudioEngine::renderBlock(float* const* output,int channels,int offset,int frames) noexcept {
    const bool active=playing.load();if(midiIO->panic.exchange(false))resumeMidi.fill(true);
    const double rate=outputRate.load();playheadTime=renderPosition;const double end=duration(),ls=loopStart.load(),le=loopEnd.load();
    const bool looping=active&&loopEnabled.load()&&le>ls&&ls>=0&&le<=end+1e-8;
    auto timeAt=[&](int frame){const double t=renderPosition+(active?frame/rate:0);return looping&&t>=le?ls+std::fmod(t-ls,le-ls):t;};
    auto automationTime=[&](int frame,int latency){const double t=seekOrigin+(elapsedSinceSeek+frame-latency)/rate;
        return looping && t>=le?ls+std::fmod(t-ls,le-ls):t;};
    bool anySolo=false;std::array<bool,maxTracks> audible{},sourceSolo{},required{};
    for(int i=0;i<count;++i){const auto& t=track(i);sourceSolo[i]=t.solo||(t.channelGroup>=0&&channelGroups[t.channelGroup].solo.load());anySolo|=sourceSolo[i];}
    // Solo on a bus also admits its upstream sources, then keeps downstream returns audible.
    if(anySolo)for(int n=count-1;n>=0;--n){const int i=processingOrder[n];const auto& t=track(i);
        const auto& destinations=compensation.targets[i];if(destinations[0]>=0 && sourceSolo[destinations[0]])sourceSolo[i]=true;
        for(int j=1;j<3;++j)if(destinations[j]>=0 && sourceSolo[destinations[j]])sourceSolo[i]=true;}
    required=sourceSolo;for(int i=0;i<count;++i)required[i]=required[i]||track(i).soloSafe.load();
    for(int n=0;n<count;++n){const int i=processingOrder[n];const auto& t=track(i);if(required[i]){
        for(int destination:compensation.targets[i])if(destination>=0)required[destination]=true;}}
    for(int i=0;i<count;++i){channelBuffers[i].clear();const auto& t=track(i);audible[i]=(stemSource.load()<0||t.auxiliary||stemSource.load()==i)&&!t.mute && !(t.vcaGroup>=0&&vcaGroups[t.vcaGroup].mute.load())&&!(t.channelGroup>=0&&channelGroups[t.channelGroup].mute.load())&&(!anySolo || required[i]);}
    masterBuffer.clear();
    std::array<float,maxTracks> peaks{},squares{};
    for(int n=0;n<count;++n){const int i=processingOrder[n];auto& t=track(i);auto& buffer=channelBuffers[i];
        const float input=std::clamp(t.inputGain.load(),0.0f,16.0f);
        if(!smoothingInitialized){smoothGain[i].setCurrentAndTargetValue(t.gain.load());smoothPan[i].setCurrentAndTargetValue(t.pan.load());smoothInput[i].setCurrentAndTargetValue(input);smoothAudible[i].setCurrentAndTargetValue(audible[i]?1.0f:0.0f);for(int j=0;j<2;++j){smoothSend[i][j].setCurrentAndTargetValue(t.sends[j].mute?0:t.sends[j].gain.load());extended->smoothSendPan[i][j].setCurrentAndTargetValue(t.sends[j].pan.load());extended->smoothSendWidth[i][j].setCurrentAndTargetValue(t.sends[j].width.load());}extended->smoothGroupGain[i].setCurrentAndTargetValue((t.vcaGroup>=0?vcaGroups[t.vcaGroup].gain.load():1.0f)*(t.channelGroup>=0?channelGroups[t.channelGroup].gain.load():1.0f));}
        smoothInput[i].setTargetValue(input);smoothAudible[i].setTargetValue(audible[i]?1.0f:0.0f);
        if(t.midiTrack){scheduleTrackMidi(i,frames,offset,active);if(t.instrument)t.instrument->process(buffer,frames,&scheduledMidi[i]);}
        std::array<float,mixBlockSize> audibility{};for(int s=0;s<frames;++s)audibility[s]=smoothAudible[i].getNextValue();
        for(int s=0;s<frames;++s){const double time=timeAt(s);float pan=t.pan.load();
            for(const auto& lane:t.automation){if(t.readsAutomation(lane)&&lane.target==-2)pan=lane.evaluate(automationTime(s,(t.auxiliary||t.midiTrack)?compensation.inputLatency[i]+(t.instrument?t.instrument->latencySamples():0):0),pan);}
            smoothPan[i].setTargetValue(std::clamp(pan,-1.0f,1.0f));
            const float p=smoothPan[i].getNextValue();const float in=smoothInput[i].getNextValue();
            if(active&&!t.auxiliary&&!t.midiTrack)for(const auto& region:t.regions){
                if(!region.asset || time<region.start || time>=region.end())continue;const auto& clip=*region.asset;
                const bool mono=clip.samples.getNumChannels()==1;
                const float lg=mono?std::sqrt((1-p)*0.5f):(p>0?1-p:1),rg=mono?std::sqrt((1+p)*0.5f):(p<0?1+p:1);
                buffer.addSample(0,s,regionSample(region,0,time-region.start)*lg);buffer.addSample(1,s,regionSample(region,mono?0:1,time-region.start)*rg);
            }
            if(monitoringAllowed&&t.monitorInput&&!t.midiTrack&&!t.auxiliary&&renderInputs&&t.recordInputLeft>=0&&t.recordInputLeft<renderInputCount&&renderInputs[t.recordInputLeft]){
                const float value=renderInputs[t.recordInputLeft][offset+s]*t.recordGain.load();const int right=t.recordStereo?t.recordInputRight:t.recordInputLeft;
                const float other=right>=0&&right<renderInputCount&&renderInputs[right]?renderInputs[right][offset+s]*t.recordGain.load():value;
                buffer.addSample(0,s,value*(p>0?1-p:1));buffer.addSample(1,s,other*(p<0?1+p:1));}
            if(t.auxiliary||t.midiTrack){if(t.monoBus){const float m=(buffer.getSample(0,s)+buffer.getSample(1,s))*0.5f;buffer.setSample(0,s,m);buffer.setSample(1,s,m);}
                buffer.setSample(0,s,buffer.getSample(0,s)*(p>0?1-p:1));buffer.setSample(1,s,buffer.getSample(1,s)*(p<0?1+p:1));}
            for(int c=0;c<2;++c)buffer.setSample(c,s,buffer.getSample(c,s)*in);
        }
        for(int s=0;s<frames;++s)if(t.sidechainSource>=0&&!t.auxiliary){const auto delayed=compensation.sidechain->sourceAlignment[i].process(buffer.getSample(0,s),buffer.getSample(1,s));buffer.setSample(0,s,delayed[0]);buffer.setSample(1,s,delayed[1]);}
        const float* detectorPointers[2]{};const float* const* detector=nullptr;if(t.sidechainSource>=0){for(int s=0;s<frames;++s){const auto d=compensation.sidechain->detectorAlignment[i].process(extended->detectorTaps[t.sidechainSource].getSample(0,s),extended->detectorTaps[t.sidechainSource].getSample(1,s));extended->detectorBuffer.setSample(0,s,d[0]);extended->detectorBuffer.setSample(1,s,d[1]);}detectorPointers[0]=extended->detectorBuffer.getReadPointer(0);detectorPointers[1]=extended->detectorBuffer.getReadPointer(1);detector=detectorPointers;}
        if(detector)for(int slot=0;slot<4;++slot)for(int s=0;s<frames;++s){const auto key=compensation.sidechain->detectorInserts[i][slot].process(detector[0][s],detector[1][s]);extended->detectorStages[slot].setSample(0,s,key[0]);extended->detectorStages[slot].setSample(1,s,key[1]);}
        // Parameter Read uses 16-sample segments; volume/pan interpolate each sample.
        const bool parameterAutomation=t.automationMode!=AutomationMode::off && t.automationMode!=AutomationMode::write && std::any_of(t.automation.begin(),t.automation.end(),[](const auto& lane){return lane.target>=0;});
        if(parameterAutomation){
            for(int s=0;s<frames;s+=16){int arrival=compensation.inputLatency[i]+(t.instrument?t.instrument->latencySamples():0);
                float* ptr[]{buffer.getWritePointer(0)+s,buffer.getWritePointer(1)+s};juce::AudioBuffer<float> segment(ptr,2,std::min(16,frames-s));
                for(int slot=0;slot<4;++slot)if(auto& insert=t.inserts[slot]){
                    for(const auto& lane:t.automation)if(t.readsAutomation(lane)&&lane.target==slot)insert->setParameter(lane.parameter,lane.evaluate(automationTime(s,arrival),insert->parameter(lane.parameter)));
                    const float* key[]{detector?extended->detectorStages[slot].getReadPointer(0)+s:nullptr,detector?extended->detectorStages[slot].getReadPointer(1)+s:nullptr};insert->process(segment,segment.getNumSamples(),nullptr,0,detector?key:nullptr);arrival+=insert->latencySamples();}}
        }else for(int slot=0;slot<4;++slot)if(auto& insert=t.inserts[slot])insert->process(buffer,frames,nullptr,0,detector?extended->detectorStages[slot].getArrayOfReadPointers():nullptr);
        for(int j=0;j<2;++j){const auto& send=t.sends[j];smoothSend[i][j].setTargetValue(send.mute?0:std::clamp(send.gain.load(),0.0f,2.0f));extended->smoothSendPan[i][j].setTargetValue(send.pan.load());extended->smoothSendWidth[i][j].setTargetValue(send.width.load());
            const int target=compensation.targets[i][j+1];if(target>=0 && send.preFader)for(int s=0;s<frames;++s){const float g=smoothSend[i][j].getNextValue()*audibility[s];
                const float left=buffer.getSample(0,s),right=buffer.getSample(1,s),middle=(left+right)*0.5f,side=(left-right)*0.5f*extended->smoothSendWidth[i][j].getNextValue(),pan=extended->smoothSendPan[i][j].getNextValue();const auto values=compensation.edges[i][j+1].process((middle+side)*g*(pan>0?1-pan:1),(middle-side)*g*(pan<0?1+pan:1));for(int c=0;c<2;++c)channelBuffers[target].addSample(c,s,values[c]);}}
        // Smooth fader after inserts/pre sends. Targets are re-evaluated at the sample's transport position.
        for(int s=0;s<frames;++s){float g=t.gain.load();for(const auto& lane:t.automation)if(t.readsAutomation(lane)&&lane.target==-1)g=lane.evaluate(automationTime(s,compensation.outputLatency[i]),g);
            smoothGain[i].setTargetValue(std::clamp(g,0.0f,2.0f));const float groupGain=(t.vcaGroup>=0?vcaGroups[t.vcaGroup].gain.load():1.0f)*(t.channelGroup>=0?channelGroups[t.channelGroup].gain.load():1.0f);extended->smoothGroupGain[i].setTargetValue(groupGain);const float f=smoothGain[i].getNextValue()*extended->smoothGroupGain[i].getNextValue();
            for(int c=0;c<2;++c){const float tap=buffer.getSample(c,s)*f;extended->detectorTaps[i].setSample(c,s,t.mute?0:tap);const float v=tap*audibility[s];buffer.setSample(c,s,v);peaks[i]=std::max(peaks[i],std::abs(v));squares[i]+=v*v;}}
        for(int j=0;j<2;++j){const auto& send=t.sends[j];const int target=compensation.targets[i][j+1];if(target>=0 && !send.preFader)for(int s=0;s<frames;++s){const float g=smoothSend[i][j].getNextValue();
                const float left=buffer.getSample(0,s),right=buffer.getSample(1,s),middle=(left+right)*0.5f,side=(left-right)*0.5f*extended->smoothSendWidth[i][j].getNextValue(),pan=extended->smoothSendPan[i][j].getNextValue();const auto values=compensation.edges[i][j+1].process((middle+side)*g*(pan>0?1-pan:1),(middle-side)*g*(pan<0?1+pan:1));for(int c=0;c<2;++c)channelBuffers[target].addSample(c,s,values[c]);}}
        const int target=compensation.targets[i][0];auto& destination=target<0?masterBuffer:channelBuffers[target];const int mode=t.outputMode.load();
        for(int s=0;s<frames;++s){const float l=buffer.getSample(0,s),r=buffer.getSample(1,s);const auto values=compensation.edges[i][0].process(mode==2?0:mode==1?(l+r)*0.5f:l,mode==1?0:mode==2?(l+r)*0.5f:r);
            destination.addSample(0,s,values[0]);destination.addSample(1,s,values[1]);}
        blockPeaks[i]=peaks[i];blockRms[i]=std::sqrt(squares[i]/(frames*2));if(peaks[i]>1)t.clipped=true;
    }
    if(active&&metronomeEnabled){const double beatSeconds=60/bpm.load()*(4.0/beatUnit);
        for(int sample=0;sample<frames;++sample){const auto beat=static_cast<int64_t>(std::floor((timeAt(sample)+1e-9)/beatSeconds));
            if(beat!=lastClickBeat){lastClickBeat=beat;clickRemaining=int(rate*0.02);clickPhase=0;}
            if(clickRemaining>0){const double frequency=beat%beatsPerBar==0?1600:1000;const float value=float(std::sin(clickPhase))*0.12f*float(clickRemaining)/(rate*0.02);
                clickPhase+=juce::MathConstants<double>::twoPi*frequency/rate;--clickRemaining;const auto delayed=clickCompensation.process(value,value);masterBuffer.addSample(0,sample,delayed[0]);masterBuffer.addSample(1,sample,delayed[1]);}
            else{const auto delayed=clickCompensation.process(0,0);masterBuffer.addSample(0,sample,delayed[0]);masterBuffer.addSample(1,sample,delayed[1]);}}}
    for(auto& insert:masterInserts)if(insert)insert->process(masterBuffer,frames);
    if(!smoothingInitialized){smoothMaster.setCurrentAndTargetValue(masterGain.load());smoothMasterAudible.setCurrentAndTargetValue(masterMute?0.0f:1.0f);}smoothingInitialized=true;smoothMaster.setTargetValue(std::clamp(masterGain.load(),0.0f,2.0f));
    smoothMasterAudible.setTargetValue(masterMute?0.0f:1.0f);
    float peak=0,square=0;
    for(int s=0;s<frames;++s){const float gain=smoothMaster.getNextValue()*smoothMasterAudible.getNextValue();float values[2]{};
        for(int c=0;c<2;++c){float v=masterBuffer.getSample(c,s)*gain;if(!std::isfinite(v) || (active&&!midiRecording&&!audioRecording&&!looping && timeAt(s)>=end+compensation.totalLatency/rate))v=0;peak=std::max(peak,std::abs(v));square+=v*v;values[c]=v;}
        if(channels==1){if(output[0])output[0][offset+s]=std::clamp((values[0]+values[1])*0.5f,-1.0f,1.0f);}
        else for(int c=0;c<2;++c)if(output[c])output[c][offset+s]=std::clamp(values[c],-1.0f,1.0f);
    }
    blockMasterPeak=peak;blockMasterRms=std::sqrt(square/(2*frames));if(peak>1)masterClipped=true;
    if(active){renderPosition=timeAt(frames);elapsedSinceSeek+=frames;if(!midiRecording&&!audioRecording&&!looping && renderPosition>=end+compensation.totalLatency/rate){renderPosition=end;playing=false;midiIO->panic=true;}}
}
void AudioEngine::attachRecorder(AudioRecorder* value){MutationGuard guard(*this);recorder=value;}
bool AudioEngine::addAudioTrack(const juce::String& name,juce::String& error){MutationGuard guard(*this);auto clip=std::make_unique<Clip>();clip->name=name;clip->samples.setSize(2,1);clip->samples.clear();if(!addClip(std::move(clip),error))return false;track(count-1).regions.clear();return true;}
bool AudioEngine::activateTake(int index,uint64_t id,juce::String& error){
    MutationGuard guard(*this);if(index<0||index>=count)return false;auto& t=track(index);const Track::Take* selected=nullptr;for(const auto& take:t.takes)if(take.id==id)selected=&take;if(!selected)return false;
    std::shared_ptr<Clip> asset=loadWav(selected->file,error);if(!asset)return false;auto before=t.regions;auto original=t.clip;
    std::erase_if(t.regions,[&](const auto& region){for(const auto& take:t.takes)if(take.id==region.takeId&&take.group==selected->group)return true;return false;});
    if(t.regions.size()>=maxRegionsPerTrack){t.regions=std::move(before);error="Limite de regioes; takes permanecem no disco.";return false;}
    t.regions.push_back({newRegionId(),asset,selected->start,0,std::min(selected->length,asset->duration()),selected->id});t.clip=asset;recalculateMediaBytes();
    if(bytes>maxSessionBytes){t.regions=std::move(before);t.clip=std::move(original);recalculateMediaBytes();error="Gravacao preservada no disco; playback excede 512 MiB.";return false;}t.activeTake=id;return true;
}
bool AudioEngine::addMidiTrack(const juce::String& name,juce::String& error){
    MutationGuard mutation(*this);auto clip=std::make_unique<Clip>();clip->name=name;clip->samples.setSize(2,1);clip->samples.clear();
    if(!addClip(std::move(clip),error))return false;auto& t=track(count-1);t.regions.clear();t.midiTrack=true;
    MidiClip midi;midi.id=newRegionId();midi.length=midiPpq*16;t.midiClips.push_back(std::move(midi));resumeMidi[count-1]=true;lastMidiTime[count-1]=-1;synchronizeMidiInputs();return rebuildMidi(count-1,error);
}
bool AudioEngine::rebuildMidi(int index,juce::String& error){
    MutationGuard mutation(*this);if(index<0||index>=count||!track(index).midiTrack){error="Faixa MIDI inexistente.";return false;}
    if(!compileMidi(track(index).midiClips,track(index).midiEvents,error))return false;resumeMidi[index]=true;return true;
}
void AudioEngine::setTempo(double value){if(!std::isfinite(value)||value<30||value>300)return;MutationGuard mutation(*this);bpm=value;bool musical=false;for(int i=0;i<count;++i)musical|=track(i).midiTrack;if(musical)seek(position());}
void AudioEngine::setTimeSignature(int beats,int unit){if(beats<1||beats>12||(unit!=4&&unit!=8))return;MutationGuard mutation(*this);beatsPerBar=beats;beatUnit=unit;playheadBeats=beats;playheadUnit=unit;lastClickBeat=-1;}
juce::Optional<juce::AudioPlayHead::PositionInfo> AudioEngine::getPosition()const{
    PositionInfo info;const double seconds=playheadTime.load(),tempo=bpm.load();const double ppq=seconds*tempo/60;
    info.setTimeInSeconds(seconds);info.setTimeInSamples(std::llround(seconds*sampleRate()));info.setBpm(tempo);info.setPpqPosition(ppq);
    const int beats=playheadBeats.load(),unit=playheadUnit.load();info.setTimeSignature(TimeSignature{beats,unit});info.setPpqPositionOfLastBarStart(std::floor(ppq/(beats*4.0/unit))*(beats*4.0/unit));
    info.setIsPlaying(isPlaying());info.setIsLooping(loopEnabled.load());return info;
}
void AudioEngine::scheduleTrackMidi(int index,int frames,int callbackOffset,bool active)noexcept{
    auto& output=scheduledMidi[index];output.clear();auto& t=track(index);const double rate=sampleRate(),tempo=bpm.load();
    const double ls=loopStart.load(),le=loopEnd.load();const bool looping=loopEnabled.load()&&le>ls;
    bool overflow=false;auto append=[&](const void* data,int bytes,int offset){if(output.getNumEvents()>=maximumMidiEventsPerBlock){overflow=true;return;}output.addEvent(data,bytes,offset);};
    if(!active){if(resumeMidi[index]){midiPanic(output);resumeMidi[index]=false;lastMidiTime[index]=-1;}}
    int cursor=0;
    while(active&&cursor<frames&&!overflow){const double unwrapped=renderPosition+cursor/rate;
        const double now=looping&&unwrapped>=le?ls+std::fmod(unwrapped-ls,le-ls):unwrapped;
        const bool resume=resumeMidi[index]||lastMidiTime[index]<0||now+1e-8<lastMidiTime[index];
        if(resume){for(int channel=0;channel<16;++channel){const uint8_t bytes[]{uint8_t(0xb0+channel),123,0};append(bytes,3,cursor);}
            const double tick=now*tempo*midiPpq/60;
            for(const auto& clip:t.midiClips)for(const auto& note:clip.notes){const auto first=clip.start+std::max(note.tick,clip.offset)-clip.offset;
                const auto last=clip.start+std::min(note.tick+note.length,clip.offset+clip.length)-clip.offset;
                if(first<tick-1e-6&&last>tick+1e-6){const uint8_t bytes[]{uint8_t(0x90+note.channel-1),uint8_t(note.pitch),uint8_t(note.velocity)};append(bytes,3,cursor);}}
            resumeMidi[index]=false;
        }
        const int span=looping?std::min(frames-cursor,std::max(1,int(std::ceil((le-now)*rate-1e-8)))):frames-cursor;
        if(!scheduleMidi(t.midiEvents,now,tempo,rate,span,midiSpan))overflow=true;
        for(const auto event:midiSpan)append(event.data,event.numBytes,cursor+event.samplePosition);
        lastMidiTime[index]=now+(span-1)/rate;cursor+=span;
    }
    for(const auto event:midiIO->live[index])if(event.samplePosition>=callbackOffset&&event.samplePosition<callbackOffset+frames)append(event.data,event.numBytes,event.samplePosition-callbackOffset);
    if(overflow){++midiDroppedBlocks;output.clear();midiPanic(output);}
}
void AudioEngine::setMidiInput(int index,const juce::String& identifier){
    MutationGuard mutation(*this);if(index<0||index>=count||!track(index).midiTrack)return;track(index).midiInput=identifier;midiIO->routes[index]=midiDeviceToken(identifier);resumeMidi[index]=true;
}
void AudioEngine::synchronizeMidiInputs(){MutationGuard mutation(*this);for(int i=0;i<maxTracks;++i)midiIO->routes[i]=i<count&&track(i).midiTrack?midiDeviceToken(track(i).midiInput):0;}
uint64_t AudioEngine::midiGeneration()const noexcept{return midiIO->generation.load();}
uint64_t AudioEngine::midiInputDrops()const noexcept{return midiIO->droppedInput.load();}
uint64_t AudioEngine::midiCaptureDrops()const noexcept{return midiIO->droppedCapture.load();}
bool AudioEngine::postMidi(int index,const juce::MidiMessage& message,int offset,uint64_t device)noexcept{
    const auto bytes=message.getRawDataSize();if(index<0||index>=maxTracks||bytes<2||bytes>3||message.getRawData()[0]<0x80||message.getRawData()[0]>=0xf0||message.getRawData()[1]>=128||(bytes==3&&message.getRawData()[2]>=128))return false;
    LiveMidiEvent event;event.generation=midiIO->generation.load();event.device=device;event.track=index;event.sampleOffset=offset;event.size=uint8_t(bytes);std::copy_n(message.getRawData(),bytes,event.bytes.begin());
    if(midiIO->incoming.tryPush(event))return true;++midiIO->droppedInput;midiIO->panic=true;return false;
}
void AudioEngine::dispatchMidiDevice(uint64_t device,const juce::MidiMessage& message)noexcept{
    if(device==0)return;const auto generation=midiIO->generation.load();double target=-1;
    const double timestamp=message.getTimeStamp(),now=juce::Time::getMillisecondCounterHiRes()*0.001;
    if(std::isfinite(timestamp)&&timestamp>0&&std::abs(timestamp-now)<2){for(int attempt=0;attempt<2;++attempt){const auto sequence=midiIO->clockSequence.load(std::memory_order_acquire);if(sequence&1)continue;
        const int frames=midiIO->clockFrames.load();const double rate=midiIO->clockRate.load();if(midiIO->clockSequence.load(std::memory_order_acquire)!=sequence)continue;if(frames>0&&rate>=100)target=timestamp+frames/rate;break;}}
const auto size=message.getRawDataSize();if(size<2||size>3||message.getRawData()[0]<0x80||message.getRawData()[0]>=0xf0||message.getRawData()[1]>=128||(size==3&&message.getRawData()[2]>=128))return;
    for(int i=0;i<maxTracks;++i)if(midiIO->routes[i].load()==device){LiveMidiEvent event;event.generation=generation;event.targetTime=target;event.device=device;event.track=i;event.size=uint8_t(size);std::copy_n(message.getRawData(),size,event.bytes.begin());
        if(!midiIO->incoming.tryPush(event)){++midiIO->droppedInput;midiIO->panic=true;}}
}
bool AudioEngine::popCapturedMidi(CapturedMidiEvent& event)noexcept{return midiIO->captured.tryPop(event);}
void AudioEngine::consumeMidiInput(int frames)noexcept{
    for(auto& buffer:midiIO->live)buffer.clear();midiIO->overflow={};const auto generation=midiIO->generation.load();
    const double wall=midiIO->renderWallTime.load(),rate=sampleRate();int retained=0;
    auto accept=[&](LiveMidiEvent event){const int index=event.track;
        if(event.generation!=generation||index<0||index>=count||!track(index).midiTrack||(event.device!=0&&midiIO->routes[index].load()!=event.device))return;
        if(event.targetTime>=0){const auto offset=std::llround((event.targetTime-wall)*rate);if(offset>=frames){if(retained<2048)midiIO->pending[retained++]=event;else{++midiIO->droppedInput;midiIO->panic=true;}return;}event.sampleOffset=int(std::max<int64_t>(0,offset));}
        auto& destination=midiIO->live[index];if(midiIO->overflow[index])return;
        if(destination.getNumEvents()>=maximumMidiEventsPerBlock){destination.clear();midiPanic(destination);midiIO->overflow[index]=true;++midiIO->droppedInput;return;}
        const int offset=std::clamp(event.sampleOffset,0,frames-1);destination.addEvent(event.bytes.data(),event.size,offset);
        if(midiRecording&&playing&&track(index).midiArm){CapturedMidiEvent capture;capture.generation=generation;capture.track=index;capture.tick=secondsToTicks(renderPosition+offset/rate,bpm.load());capture.bytes=event.bytes;capture.size=event.size;
            if(!midiIO->captured.tryPush(capture)){++midiIO->droppedCapture;midiRecording=false;midiIO->panic=true;}}
    };
    const int previous=midiIO->pendingCount;for(int i=0;i<previous;++i)accept(midiIO->pending[i]);
    LiveMidiEvent event;for(int received=0;received<2048&&midiIO->incoming.tryPop(event);++received)accept(event);midiIO->pendingCount=retained;
}
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* input, int inputs,
    float* const* out, int channels, int samples, const juce::AudioIODeviceCallbackContext&) {
    const bool measure=measureDeviceCallbacks.load(std::memory_order_relaxed);
    const auto before=measure?juce::Time::getHighResolutionTicks():0;
    renderWithInput(input,inputs,out, channels, samples);
    if(measure){const auto ticks=uint64_t(juce::Time::getHighResolutionTicks()-before);const double us=double(ticks)*1e6/juce::Time::getHighResolutionTicksPerSecond();
        ++deviceTimingHistogram[std::min<size_t>(127,size_t(us/100))];deviceTotalTicks+=ticks;if(ticks>deviceMaxTicks.load(std::memory_order_relaxed))deviceMaxTicks=ticks;
        if(us>samples/requestedDeviceRate.load()*1e6)++deviceDeadlineOverruns;++deviceCallbackCount;}
}
juce::var AudioEngine::deviceCallbackTiming()const {
    auto result=std::make_unique<juce::DynamicObject>();const auto callbacks=deviceCallbackCount.load();const double factor=1e6/juce::Time::getHighResolutionTicksPerSecond();
    result->setProperty("callbacks",int64_t(callbacks));result->setProperty("overruns",int64_t(deviceDeadlineOverruns.load()));result->setProperty("maxUs",deviceMaxTicks.load()*factor);result->setProperty("meanUs",callbacks?deviceTotalTicks.load()*factor/callbacks:0);
    for(auto percentile:{50,95,99}){uint64_t accumulated=0;double upper=0;for(size_t bin=0;bin<deviceTimingHistogram.size();++bin){accumulated+=deviceTimingHistogram[bin].load();if(callbacks&&accumulated>=((callbacks*percentile+99)/100)){upper=bin==127?-1:double(bin+1)*100;break;}}result->setProperty("p"+juce::String(percentile)+"UpperUs",upper);}
    result->setProperty("histogramOverflowCount",int64_t(deviceTimingHistogram.back().load()));return juce::var(result.release());
}
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) { monitoringAllowed=false;requestedDeviceRate.store(device->getCurrentSampleRate()); }
void AudioEngine::audioDeviceStopped() { monitoringAllowed=false;pause(); }
AudioRuntime::AudioRuntime() { audioRecorder=std::make_unique<AudioRecorder>(engine);engine.attachRecorder(audioRecorder.get());midiRecorder=std::make_unique<MidiRecorder>(engine);initialError = devices.initialiseWithDefaultDevices(0, 2);if(auto* device=devices.getCurrentAudioDevice())engine.setOutputRate(device->getCurrentSampleRate()); devices.addAudioCallback(&engine); }
void AudioRuntime::serviceMaintenance(){if(!engine.monitoringAllowed)for(int i=0;i<engine.trackCount();++i)engine.track(i).monitorInput=false;audioRecorder->service();midiRecorder->service();engine.automation->service();engine.mastering->service();if(engine.needsMaintenance())editStructure([this]{if(auto* device=devices.getCurrentAudioDevice())engine.setOutputRate(device->getCurrentSampleRate());else engine.prepareProcessors();});}
AudioRuntime::~AudioRuntime() { midiPorts.clear();devices.removeAudioCallback(&engine);juce::String error;audioRecorder->stop(error);midiRecorder->stop(error);devices.closeAudioDevice(); }
bool AudioRuntime::ensureMidiInput(const juce::String& identifier,juce::String& error){
    if(identifier.isEmpty())return true;for(const auto& port:midiPorts)if(port->identifier==identifier)return true;
    auto port=std::make_unique<MidiPort>(engine,identifier);if(!port->available()){error="Dispositivo MIDI indisponivel: "+identifier;return false;}midiPorts.push_back(std::move(port));return true;
}
juce::String AudioRuntime::deviceDescription() const {
    if (auto* device = devices.getCurrentAudioDevice())
        return device->getName() + " | " + juce::String(device->getCurrentSampleRate(), 0)
            + " Hz | " + juce::String(device->getCurrentBufferSizeSamples()) + " samples";
    return "Sem dispositivo de saida: " + initialError;
}
}
