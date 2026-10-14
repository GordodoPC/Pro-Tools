#include "AudioEngine.h"
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
    for(int i=0;i<count;++i) { assets.insert(track(i).clip.get()); for(const auto& r:track(i).regions)assets.insert(r.asset.get()); }
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
    t.auxiliary=false;t.monoBus=false;t.outputTarget=-1;t.inputGain=1;t.rms=0;t.clipped=false;t.automation.clear();t.automationMode=AutomationMode::off;
    for(auto& p:t.inserts)p.reset();for(auto& send:t.sends){send.target=-1;send.gain=0;send.preFader=false;}
    t.clip = std::move(clip); t.gain = 1; t.pan = 0; t.mute = false; t.solo = false; t.peak = 0;
    t.name = t.clip->name; t.height = 140; t.outputMode = 0;
    t.colour = juce::Colour::fromHSV(std::fmod(0.47f + (count - 1) * 0.13f, 1.0f), 0.5f, 0.88f, 1.0f).getARGB();
    t.regions = {{newRegionId(), t.clip, 0, 0, t.clip->duration()}};
    bytes += clipBytes; processingOrder[count-1]=count-1;smoothingInitialized=false;
    if(!rebuildGraph(error)){--count;bytes-=clipBytes;t.regions.clear();t.clip.reset();return false;}return true;
}
void AudioEngine::clear() {
    MutationGuard mutation(*this);
    stop();
    for (auto& t : tracks) { t.regions.clear(); t.clip.reset();for(auto& p:t.inserts)p.reset();t.automation.clear(); }
    for(auto& p:masterInserts)p.reset();masterMute=false;masterClipped=false;masterPeak=masterRms=0;smoothingInitialized=false;
    count = 0; bytes = 0; masterGain = 1;compensation={};visibleLatency=0;pdcReady=true;configurationError.clear();
    loopEnabled = false; loopStart = 0; loopEnd = 0; bpm = 120; beatsPerBar = beatUnit = 4;
}
double AudioEngine::duration() const {
    double end = 0;
    for (int i = 0; i < count; ++i) for (const auto& region : tracks[static_cast<size_t>(i)].regions) end = std::max(end, region.end());
    return end;
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
    const double value = std::clamp(seconds, 0.0, duration());
    for(int i=0;i<count;++i)for(auto& insert:track(i).inserts)if(insert)insert->reset();
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
AudioEngine::AudioEngine() {
    for(auto& buffer:channelBuffers)buffer.setSize(2,mixBlockSize);
    masterBuffer.setSize(2,mixBlockSize);setSmoothingTime(smoothingTime);
}
void AudioEngine::setSmoothingTime(double seconds) {
    MutationGuard mutation(*this);
    smoothingTime=std::clamp(seconds,0.0,0.1);smoothingInitialized=false;
    for(int i=0;i<maxTracks;++i){smoothGain[i].reset(outputRate,smoothingTime);smoothPan[i].reset(outputRate,smoothingTime);smoothInput[i].reset(outputRate,smoothingTime);smoothAudible[i].reset(outputRate,smoothingTime);for(auto& send:smoothSend[i])send.reset(outputRate,smoothingTime);}
    smoothMaster.reset(outputRate,smoothingTime);smoothMasterAudible.reset(outputRate,smoothingTime);
}
void AudioEngine::setOutputRate(double rate) {
    MutationGuard mutation(*this);
    outputRate=std::isfinite(rate)&&rate>=100?rate:44100;requestedDeviceRate=outputRate.load();setSmoothingTime(smoothingTime);prepareProcessors();
}
void AudioEngine::prepareProcessors() {
    MutationGuard mutation(*this);
    try{
        for(int i=0;i<count;++i)for(auto& insert:track(i).inserts)if(insert)insert->prepare(outputRate);
        for(auto& insert:masterInserts)if(insert)insert->prepare(outputRate);
        juce::String error;if(!rebuildGraph(error)){configurationError=error;pdcReady=false;}
    }catch(const std::exception& ex){configurationError="Falha ao preparar processamento: "+juce::String(ex.what());pdcReady=false;}
}
bool AudioEngine::needsMaintenance()const {
    if(requestedDeviceRate.load()!=outputRate.load())return true;
    for(int i=0;i<count;++i)for(const auto& insert:track(i).inserts)if(insert && insert->latencyChanged())return true;
    for(const auto& insert:masterInserts)if(insert && insert->latencyChanged())return true;return false;
}
uint64_t AudioEngine::contentionDropouts()const {
    uint64_t total=0;for(int i=0;i<count;++i)for(const auto& insert:track(i).inserts)if(insert)total+=insert->contentionDropouts();
    for(const auto& insert:masterInserts)if(insert)total+=insert->contentionDropouts();return total;
}
void AudioEngine::closeEditors(){for(int i=0;i<count;++i)for(auto& p:track(i).inserts)if(p)p->closeEditor();for(auto& p:masterInserts)if(p)p->closeEditor();}
void AudioEngine::replaceSession(AudioEngine& other) {
    MutationGuard mutation(*this);
    clear();
    for(int i=0;i<other.count;++i){auto& src=other.track(i);auto& dst=track(i);
        dst.clip=std::move(src.clip);dst.regions=std::move(src.regions);dst.name=src.name;dst.colour=src.colour;dst.height=src.height;
        dst.gain=src.gain.load();dst.pan=src.pan.load();dst.mute=src.mute.load();dst.solo=src.solo.load();dst.outputMode=src.outputMode.load();
        dst.inputGain=src.inputGain.load();dst.outputTarget=src.outputTarget;dst.auxiliary=src.auxiliary;dst.monoBus=src.monoBus;
        dst.peak=dst.rms=0;dst.clipped=false;dst.inserts=std::move(src.inserts);dst.automation=std::move(src.automation);dst.automationMode=src.automationMode;
        for(int j=0;j<2;++j){dst.sends[j].target=src.sends[j].target;dst.sends[j].preFader=src.sends[j].preFader;dst.sends[j].gain=src.sends[j].gain.load();}
    }
    count=other.count;bytes=other.bytes;nextRegionId=other.nextRegionId;processingOrder=other.processingOrder;
    masterGain=other.masterGain.load();masterMute=other.masterMute.load();masterInserts=std::move(other.masterInserts);
    compensation=std::move(other.compensation);visibleLatency=compensation.totalLatency;pdcReady=other.pdcReady.load();configurationError=other.configurationError;
    bpm=other.bpm;beatsPerBar=other.beatsPerBar;beatUnit=other.beatUnit;
    loopStart=other.loopStart.load();loopEnd=other.loopEnd.load();loopEnabled=other.loopEnabled.load();
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
    for(int i=0;i<count;++i){const auto& t=track(i);int latency=0;for(const auto& insert:t.inserts)if(insert)latency+=insert->latencySamples();
        nodes.push_back({t.auxiliary,t.outputTarget,latency,{t.sends[0].target,t.sends[1].target}});}
    int masterLatency=0;for(const auto& insert:masterInserts)if(insert)masterLatency+=insert->latencySamples();
    DelayCompensation candidate;if(!candidate.build(nodes,masterLatency,error))return false;
    compensation=std::move(candidate);processingOrder=compensation.order;visibleLatency=compensation.totalLatency;pdcReady=true;configurationError.clear();return true;
}
bool AudioEngine::setRoute(int source,int destination,juce::String& error) {
    MutationGuard mutation(*this);
    if(source<0 || source>=count)return false;auto& t=track(source);const int old=t.outputTarget;t.outputTarget=destination;
    if(rebuildGraph(error))return true;t.outputTarget=old;return false;
}
bool AudioEngine::setSend(int source,int slot,int destination,bool pre,juce::String& error) {
    MutationGuard mutation(*this);
    if(source<0 || source>=count || slot<0 || slot>=2)return false;
    auto& send=track(source).sends[slot];const int old=send.target;const bool oldPre=send.preFader;send.target=destination;send.preFader=pre;
    if(rebuildGraph(error))return true;send.target=old;send.preFader=oldPre;return false;
}
void AudioEngine::render(float* const* output,int channels,int frames) noexcept {
    if(!output || channels<1 || frames<1)return;
    for(int c=0;c<channels;++c)if(output[c])std::fill_n(output[c],frames,0.0f);
    unsigned expected=0;if(!processingGate.compare_exchange_strong(expected,1,std::memory_order_acquire))return;
    struct Release {std::atomic<unsigned>& gate;~Release(){gate.fetch_sub(1,std::memory_order_release);}} release{processingGate};
    // A rate/latency transition is serviced off-thread; emit silence instead of misaligned audio.
    if(!pdcReady.load() || needsMaintenance())return;
    const auto generation=seekGeneration.load(std::memory_order_acquire);
    if(generation!=consumedSeek){renderPosition=requestedPosition.load(std::memory_order_relaxed);consumedSeek=generation;seekOrigin=renderPosition;elapsedSinceSeek=0;compensation.reset();
        // Seeking resets native tails. VST3 reset is a lifecycle operation and stays off the callback.
        for(int i=0;i<count;++i)for(auto& p:track(i).inserts)if(p)p->reset(false);for(auto& p:masterInserts)if(p)p->reset(false);}
    if(!playing){masterPeak=masterRms=0;for(int i=0;i<count;++i)track(i).peak=track(i).rms=0;return;}
    float masterEnergy=0,masterMaximum=0;std::array<float,maxTracks> channelEnergy{},channelMaximum{};
    for(int offset=0;offset<frames;offset+=mixBlockSize){const int size=std::min(mixBlockSize,frames-offset);renderBlock(output,channels,offset,size);
        masterEnergy+=blockMasterRms*blockMasterRms*size;masterMaximum=std::max(masterMaximum,blockMasterPeak);
        for(int i=0;i<count;++i){channelEnergy[i]+=blockRms[i]*blockRms[i]*size;channelMaximum[i]=std::max(channelMaximum[i],blockPeaks[i]);}}
    // Publish meters after the full callback; UI readers never see an intermediate clear.
    masterPeak=masterMaximum;masterRms=std::sqrt(masterEnergy/frames);for(int i=0;i<count;++i){track(i).peak=channelMaximum[i];track(i).rms=std::sqrt(channelEnergy[i]/frames);}
    visiblePosition=std::min(renderPosition,duration());
}
void AudioEngine::renderBlock(float* const* output,int channels,int offset,int frames) noexcept {
    const double rate=outputRate.load();const double end=duration(),ls=loopStart.load(),le=loopEnd.load();
    const bool looping=loopEnabled.load()&&le>ls&&ls>=0&&le<=end+1e-8;
    auto timeAt=[&](int frame){const double t=renderPosition+frame/rate;return looping&&t>=le?ls+std::fmod(t-ls,le-ls):t;};
    auto automationTime=[&](int frame,int latency){const double t=seekOrigin+(elapsedSinceSeek+frame-latency)/rate;
        return looping && t>=le?ls+std::fmod(t-ls,le-ls):t;};
    bool anySolo=false;std::array<bool,maxTracks> audible{},sourceSolo{},required{};
    for(int i=0;i<count;++i){sourceSolo[i]=track(i).solo;anySolo|=sourceSolo[i];}
    // Solo on a bus also admits its upstream sources, then keeps downstream returns audible.
    if(anySolo)for(int n=count-1;n>=0;--n){const int i=processingOrder[n];const auto& t=track(i);
        const auto& destinations=compensation.targets[i];if(destinations[0]>=0 && sourceSolo[destinations[0]])sourceSolo[i]=true;
        for(int j=1;j<3;++j)if(destinations[j]>=0 && sourceSolo[destinations[j]])sourceSolo[i]=true;}
    required=sourceSolo;
    for(int n=0;n<count;++n){const int i=processingOrder[n];const auto& t=track(i);if(required[i]){
        for(int destination:compensation.targets[i])if(destination>=0)required[destination]=true;}}
    for(int i=0;i<count;++i){channelBuffers[i].clear();audible[i]=!track(i).mute && (!anySolo || required[i]);}
    masterBuffer.clear();
    std::array<float,maxTracks> peaks{},squares{};
    for(int n=0;n<count;++n){const int i=processingOrder[n];auto& t=track(i);auto& buffer=channelBuffers[i];
        const float input=std::clamp(t.inputGain.load(),0.0f,16.0f);
        if(!smoothingInitialized){smoothGain[i].setCurrentAndTargetValue(t.gain.load());smoothPan[i].setCurrentAndTargetValue(t.pan.load());smoothInput[i].setCurrentAndTargetValue(input);smoothAudible[i].setCurrentAndTargetValue(audible[i]?1.0f:0.0f);for(int j=0;j<2;++j)smoothSend[i][j].setCurrentAndTargetValue(t.sends[j].gain.load());}
        smoothInput[i].setTargetValue(input);smoothAudible[i].setTargetValue(audible[i]?1.0f:0.0f);
        std::array<float,mixBlockSize> audibility{};for(int s=0;s<frames;++s)audibility[s]=smoothAudible[i].getNextValue();
        for(int s=0;s<frames;++s){const double time=timeAt(s);float pan=t.pan.load();
            if(t.automationMode==AutomationMode::read)for(const auto& lane:t.automation){if(lane.target==-2)pan=lane.evaluate(automationTime(s,compensation.inputLatency[i]),pan);}
            smoothPan[i].setTargetValue(std::clamp(pan,-1.0f,1.0f));
            const float p=smoothPan[i].getNextValue();const float in=smoothInput[i].getNextValue();
            if(!t.auxiliary)for(const auto& region:t.regions){
                if(!region.asset || time<region.start || time>=region.end())continue;const auto& clip=*region.asset;
                const double pos=(time-region.start+region.offset)*clip.sampleRate;
                if(pos<0 || pos>=clip.samples.getNumSamples())continue;
                const int a=static_cast<int>(pos),b=std::min(a+1,clip.samples.getNumSamples()-1);const float frac=static_cast<float>(pos-a);
                const bool mono=clip.samples.getNumChannels()==1;
                const float lg=mono?std::sqrt((1-p)*0.5f):(p>0?1-p:1),rg=mono?std::sqrt((1+p)*0.5f):(p<0?1+p:1);
                const auto* l=clip.samples.getReadPointer(0);const auto* r=clip.samples.getReadPointer(mono?0:1);
                buffer.addSample(0,s,(l[a]+(l[b]-l[a])*frac)*lg);buffer.addSample(1,s,(r[a]+(r[b]-r[a])*frac)*rg);
            }
            if(t.auxiliary){if(t.monoBus){const float m=(buffer.getSample(0,s)+buffer.getSample(1,s))*0.5f;buffer.setSample(0,s,m);buffer.setSample(1,s,m);}
                buffer.setSample(0,s,buffer.getSample(0,s)*(p>0?1-p:1));buffer.setSample(1,s,buffer.getSample(1,s)*(p<0?1+p:1));}
            for(int c=0;c<2;++c)buffer.setSample(c,s,buffer.getSample(c,s)*in);
        }
        // Parameter Read uses 16-sample segments; volume/pan interpolate each sample.
        const bool parameterAutomation=t.automationMode==AutomationMode::read && std::any_of(t.automation.begin(),t.automation.end(),[](const auto& lane){return lane.target>=0;});
        if(parameterAutomation){
            for(int s=0;s<frames;s+=16){int arrival=compensation.inputLatency[i];
                float* ptr[]{buffer.getWritePointer(0)+s,buffer.getWritePointer(1)+s};juce::AudioBuffer<float> segment(ptr,2,std::min(16,frames-s));
                for(int slot=0;slot<4;++slot)if(auto& insert=t.inserts[slot]){
                    for(const auto& lane:t.automation)if(lane.target==slot)insert->setParameter(lane.parameter,lane.evaluate(automationTime(s,arrival),insert->parameter(lane.parameter)));
                    insert->process(segment,segment.getNumSamples());arrival+=insert->latencySamples();}}
        }else for(auto& insert:t.inserts)if(insert)insert->process(buffer,frames);
        for(int j=0;j<2;++j){const auto& send=t.sends[j];smoothSend[i][j].setTargetValue(std::clamp(send.gain.load(),0.0f,2.0f));
            const int target=compensation.targets[i][j+1];if(target>=0 && send.preFader)for(int s=0;s<frames;++s){const float g=smoothSend[i][j].getNextValue()*audibility[s];
                const auto values=compensation.edges[i][j+1].process(buffer.getSample(0,s)*g,buffer.getSample(1,s)*g);for(int c=0;c<2;++c)channelBuffers[target].addSample(c,s,values[c]);}}
        // Smooth fader after inserts/pre sends. Targets are re-evaluated at the sample's transport position.
        for(int s=0;s<frames;++s){float g=t.gain.load();if(t.automationMode==AutomationMode::read)for(const auto& lane:t.automation)if(lane.target==-1)g=lane.evaluate(automationTime(s,compensation.outputLatency[i]),g);
            smoothGain[i].setTargetValue(std::clamp(g,0.0f,2.0f));const float f=smoothGain[i].getNextValue();
            for(int c=0;c<2;++c){const float v=buffer.getSample(c,s)*f*audibility[s];buffer.setSample(c,s,v);peaks[i]=std::max(peaks[i],std::abs(v));squares[i]+=v*v;}}
        for(int j=0;j<2;++j){const auto& send=t.sends[j];const int target=compensation.targets[i][j+1];if(target>=0 && !send.preFader)for(int s=0;s<frames;++s){const float g=smoothSend[i][j].getNextValue();
                const auto values=compensation.edges[i][j+1].process(buffer.getSample(0,s)*g,buffer.getSample(1,s)*g);for(int c=0;c<2;++c)channelBuffers[target].addSample(c,s,values[c]);}}
        const int target=compensation.targets[i][0];auto& destination=target<0?masterBuffer:channelBuffers[target];const int mode=t.outputMode.load();
        for(int s=0;s<frames;++s){const float l=buffer.getSample(0,s),r=buffer.getSample(1,s);const auto values=compensation.edges[i][0].process(mode==2?0:mode==1?(l+r)*0.5f:l,mode==1?0:mode==2?(l+r)*0.5f:r);
            destination.addSample(0,s,values[0]);destination.addSample(1,s,values[1]);}
        blockPeaks[i]=peaks[i];blockRms[i]=std::sqrt(squares[i]/(frames*2));if(peaks[i]>1)t.clipped=true;
    }
    for(auto& insert:masterInserts)if(insert)insert->process(masterBuffer,frames);
    if(!smoothingInitialized){smoothMaster.setCurrentAndTargetValue(masterGain.load());smoothMasterAudible.setCurrentAndTargetValue(masterMute?0.0f:1.0f);}smoothingInitialized=true;smoothMaster.setTargetValue(std::clamp(masterGain.load(),0.0f,2.0f));
    smoothMasterAudible.setTargetValue(masterMute?0.0f:1.0f);
    float peak=0,square=0;
    for(int s=0;s<frames;++s){const float gain=smoothMaster.getNextValue()*smoothMasterAudible.getNextValue();float values[2]{};
        for(int c=0;c<2;++c){float v=masterBuffer.getSample(c,s)*gain;if(!std::isfinite(v) || (!looping && timeAt(s)>=end+compensation.totalLatency/rate))v=0;peak=std::max(peak,std::abs(v));square+=v*v;values[c]=v;}
        if(channels==1){if(output[0])output[0][offset+s]=std::clamp((values[0]+values[1])*0.5f,-1.0f,1.0f);}
        else for(int c=0;c<2;++c)if(output[c])output[c][offset+s]=std::clamp(values[c],-1.0f,1.0f);
    }
    blockMasterPeak=peak;blockMasterRms=std::sqrt(square/(2*frames));if(peak>1)masterClipped=true;
    renderPosition=timeAt(frames);elapsedSinceSeek+=frames;if(!looping && renderPosition>=end+compensation.totalLatency/rate){renderPosition=end;playing=false;}
}
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*, int,
    float* const* out, int channels, int samples, const juce::AudioIODeviceCallbackContext&) {
    render(out, channels, samples);
}
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) { requestedDeviceRate.store(device->getCurrentSampleRate()); }
void AudioEngine::audioDeviceStopped() { pause(); }
AudioRuntime::AudioRuntime() { initialError = devices.initialiseWithDefaultDevices(0, 2);if(auto* device=devices.getCurrentAudioDevice())engine.setOutputRate(device->getCurrentSampleRate()); devices.addAudioCallback(&engine); }
void AudioRuntime::serviceMaintenance(){if(engine.needsMaintenance())editStructure([this]{if(auto* device=devices.getCurrentAudioDevice())engine.setOutputRate(device->getCurrentSampleRate());else engine.prepareProcessors();});}
AudioRuntime::~AudioRuntime() { devices.removeAudioCallback(&engine); devices.closeAudioDevice(); }
juce::String AudioRuntime::deviceDescription() const {
    if (auto* device = devices.getCurrentAudioDevice())
        return device->getName() + " | " + juce::String(device->getCurrentSampleRate(), 0)
            + " Hz | " + juce::String(device->getCurrentBufferSizeSamples()) + " samples";
    return "Sem dispositivo de saida: " + initialError;
}
}
