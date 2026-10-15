#include "AudioRecorder.h"
#include "AudioEngine.h"
#include "Midi/MidiQueue.h"
#include <algorithm>
#include <thread>
#include <cmath>
namespace nova {
namespace {
struct RecordPacket {std::array<std::array<float,256>,2> samples{};int frames=0,epoch=0;double start=0;};
enum RecordError {none,queueFull,inputLost,writeFailed,takeLimit};
struct RecordLane {
    int track=0,left=0,right=1,channels=1;uint64_t group=0;
    MidiQueue<RecordPacket,256> queue;RecordPacket scratch;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    std::vector<Track::Take> files;
    int epoch=-1;int64_t framesWritten=0;
};
}
struct AudioRecorder::Impl {
    AudioEngine& engine;double rate=48000;uint64_t generation=0;RecordingOptions options;juce::File folder;
    std::vector<std::unique_ptr<RecordLane>> lanes;std::thread worker;
    std::atomic<bool> active{false},stop{false};std::atomic<int> error{none};std::atomic<uint64_t> overflows{0},written{0};
    double lastTime=-1;int epoch=0;uint64_t group=0;
    explicit Impl(AudioEngine& e):engine(e){}
    ~Impl(){active=false;stop=true;if(worker.joinable())worker.join();}
    void fail(int reason)noexcept{int expected=none;error.compare_exchange_strong(expected,reason);active=false;engine.audioRecording=false;}
    void flush(RecordLane& lane)noexcept{if(lane.scratch.frames>0){if(!lane.queue.tryPush(lane.scratch)){++overflows;fail(queueFull);}lane.scratch.frames=0;}}
    void journal(const char* state){auto root=std::make_unique<juce::DynamicObject>();root->setProperty("format","nova-recording-recovery");root->setProperty("state",state);root->setProperty("sampleRate",rate);root->setProperty("error",error.load());juce::Array<juce::var> files;
        for(const auto& lane:lanes)for(const auto& take:lane->files){auto item=std::make_unique<juce::DynamicObject>();item->setProperty("file",take.file.getFileName());item->setProperty("track",lane->track);item->setProperty("start",take.start);item->setProperty("length",take.length);item->setProperty("channels",take.channels);files.add(juce::var(item.release()));}
        root->setProperty("files",files);juce::TemporaryFile temporary(folder.getChildFile("recovery.json"));if(temporary.getFile().replaceWithText(juce::JSON::toString(juce::var(root.release()),false)))temporary.overwriteTargetFileWithTemporary();
    }
    bool open(RecordLane& lane,const RecordPacket& packet){lane.writer.reset();if(lane.files.size()>=128){fail(takeLimit);return false;}
        const auto file=folder.getChildFile("track-"+juce::String(lane.track+1)+"-take-"+juce::String(packet.epoch+1)+".wav");auto output=file.createOutputStream();juce::WavAudioFormat format;
        if(!output){fail(writeFailed);return false;}lane.writer.reset(format.createWriterFor(output.release(),rate,unsigned(lane.channels),24,{},0));if(!lane.writer){fail(writeFailed);return false;}
        lane.files.push_back({0,group,file,packet.start,0,rate,lane.channels});lane.epoch=packet.epoch;lane.framesWritten=0;journal("capturing");return true;
    }
    void run(){try{juce::AudioBuffer<float> buffer(2,256);RecordPacket packet;
        for(;;){bool consumed=false;for(auto& lane:lanes)for(int n=0;n<256&&lane->queue.tryPop(packet);++n){consumed=true;if(error.load()==writeFailed)continue;
            if(lane->epoch!=packet.epoch&&!open(*lane,packet))continue;
            if(options.failAfterFrames>=0&&int64_t(written.load())>=options.failAfterFrames){fail(writeFailed);continue;}
            if(lane->framesWritten+packet.frames>int64_t(0xffff0000ull/(lane->channels*3))){fail(writeFailed);continue;}
            for(int channel=0;channel<lane->channels;++channel)std::copy_n(packet.samples[channel].data(),packet.frames,buffer.getWritePointer(channel));
            if(!lane->writer->writeFromAudioSampleBuffer(buffer,0,packet.frames)){fail(writeFailed);continue;}lane->framesWritten+=packet.frames;written+=packet.frames;lane->files.back().length=lane->framesWritten/rate;
            if(lane->framesWritten%4096<packet.frames){if(!lane->writer->flush())fail(writeFailed);journal("capturing");}
        }
        if(stop&&!consumed)break;if(!consumed)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        for(auto& lane:lanes)lane->writer.reset();journal(error.load()==none?"closed":"interrupted");
    }catch(...){fail(writeFailed);for(auto& lane:lanes)lane->writer.reset();}}
};
AudioRecorder::AudioRecorder(AudioEngine& owner):engine(owner){}
AudioRecorder::~AudioRecorder(){juce::String error;stop(error);}
bool AudioRecorder::isActive()const noexcept{return impl&&impl->active.load();}
uint64_t AudioRecorder::overflowCount()const noexcept{return impl?impl->overflows.load():0;}
juce::File AudioRecorder::captureDirectory()const{return impl?impl->folder:juce::File{};}
juce::String AudioRecorder::status()const{return isActive()?"Gravando WAV; "+juce::String(impl->written.load())+" samples":lastStatus;}
bool AudioRecorder::start(const juce::File& destination,int availableInputs,const RecordingOptions& options,juce::String& error){
    error.clear();if(impl&&impl->worker.joinable()){error="Finalize a gravacao anterior.";return false;}
    if(!std::isfinite(options.punchIn)||!std::isfinite(options.punchOut)||!std::isfinite(options.preRoll)||!std::isfinite(options.postRoll)||!std::isfinite(options.loopStart)||!std::isfinite(options.loopEnd)
        ||options.punchIn<0||options.punchOut>86400||options.punchOut<=options.punchIn||options.preRoll<0||options.preRoll>60||options.postRoll<0||options.postRoll>60||options.loopStart<0||options.loopEnd<=options.loopStart||options.loopEnd>86400){error="Intervalos de gravacao invalidos.";return false;}
    AudioEngine::MutationGuard guard(engine);auto next=std::make_unique<Impl>(engine);next->rate=engine.sampleRate();next->generation=engine.midiGeneration();next->options=options;next->group=engine.newRegionId();
    for(int i=0;i<engine.trackCount();++i){const auto& track=engine.track(i);if(track.auxiliary||track.midiTrack||!track.recordArm)continue;
        if(track.recordInputLeft<0||track.recordInputLeft>=availableInputs||(track.recordStereo&&(track.recordInputRight<0||track.recordInputRight>=availableInputs))){error="Ative e selecione as entradas de audio antes de gravar.";return false;}
        if(next->lanes.size()>=32){error="Limite de 32 faixas armadas por captura.";return false;}
        auto lane=std::make_unique<RecordLane>();lane->track=i;lane->left=track.recordInputLeft;lane->right=track.recordInputRight;lane->channels=track.recordStereo?2:1;next->lanes.push_back(std::move(lane));
    }
    if(next->lanes.empty()){error="Arme pelo menos uma faixa de audio.";return false;}
    next->folder=destination.getChildFile("capture-"+juce::Uuid().toString());if(!next->folder.createDirectory().wasOk()){error="Pasta de gravacao indisponivel.";return false;}
    impl=std::move(next);engine.recordingOptions=options;engine.recordingOptions.failAfterFrames=-1;engine.recordingExtent=options.loop?options.loopEnd:options.punch?options.punchOut+options.postRoll:0;
    if(options.loop){engine.setLoop(options.loopStart,options.loopEnd,true);engine.seek(options.loopStart);}else if(options.punch){engine.loopEnabled=false;engine.seek(std::max(0.0,options.punchIn-options.preRoll));}
    else engine.loopEnabled=false;
    impl->worker=std::thread([state=impl.get()]{state->run();});impl->active=true;engine.audioRecording=true;engine.play();lastStatus="Captura em "+impl->folder.getFullPathName();return true;
}
void AudioRecorder::capture(const float* const* input,int channels,int frames,double seconds)noexcept{
    if(!impl||!impl->active||!engine.audioRecording)return;auto& state=*impl;
    if(state.generation!=engine.midiGeneration()||engine.sampleRate()!=state.rate){state.fail(inputLost);return;}
    for(const auto& lane:state.lanes)if(!input||lane->left>=channels||!input[lane->left]||(lane->channels==2&&(lane->right>=channels||!input[lane->right]))){state.fail(inputLost);return;}
    for(int sample=0;sample<frames&&state.active;++sample){double time=seconds+sample/state.rate;if(state.options.loop&&time>=state.options.loopEnd)time=state.options.loopStart+std::fmod(time-state.options.loopStart,state.options.loopEnd-state.options.loopStart);
        if(state.lastTime>=0&&time+1e-8<state.lastTime){for(auto& lane:state.lanes)state.flush(*lane);if(++state.epoch>=128){state.fail(takeLimit);break;}}state.lastTime=time;
        if(state.options.punch&&(time<state.options.punchIn||time>=state.options.punchOut)){for(auto& lane:state.lanes)state.flush(*lane);if(!state.options.loop&&time>=state.options.punchOut+state.options.postRoll){state.active=false;engine.audioRecording=false;}continue;}
        for(auto& lane:state.lanes){auto& packet=lane->scratch;if(packet.frames==0){packet.start=time;packet.epoch=state.epoch;}
            const float gain=std::clamp(engine.track(lane->track).recordGain.load(),0.0f,16.0f);for(int channel=0;channel<lane->channels;++channel){const float value=input[channel==0?lane->left:lane->right][sample]*gain;packet.samples[channel][packet.frames]=std::isfinite(value)?value:0;}
            if(++packet.frames==256)state.flush(*lane);
        }
    }
    for(auto& lane:state.lanes)state.flush(*lane);
}
bool AudioRecorder::stop(juce::String& error){
    error.clear();if(!impl||!impl->worker.joinable())return true;AudioEngine::MutationGuard guard(engine);impl->active=false;engine.audioRecording=false;for(auto& lane:impl->lanes)impl->flush(*lane);impl->stop=true;impl->worker.join();engine.recordingExtent=0;
    bool ok=impl->error.load()==none;const bool same=impl->generation==engine.midiGeneration();
    for(auto& lane:impl->lanes)if(same&&lane->track<engine.trackCount()){auto& track=engine.track(lane->track);uint64_t last=0;
        for(auto take:lane->files)if(take.length>0){if(track.takes.size()>=128){ok=false;continue;}take.id=engine.newRegionId();last=take.id;track.takes.push_back(std::move(take));}
        if(last){juce::String adoption;if(!engine.activateTake(lane->track,last,adoption)){ok=false;error=adoption;}}
    }
    if(!ok&&error.isEmpty())error="Captura interrompida (entrada, fila ou disco); WAV recebido e recovery.json preservados.";
    lastStatus=ok?"Gravacao concluida; WAV/takes preservados":"Gravacao parcial: "+error;if(same)engine.pause();return ok;
}
void AudioRecorder::service(){if(impl&&impl->worker.joinable()&&(!impl->active||!engine.audioRecording||!engine.isPlaying()||impl->generation!=engine.midiGeneration())){juce::String error;stop(error);}}
bool AudioRecorder::recover(const juce::File& directory,juce::String& error){
    error.clear();if(isActive()){error="Pare a gravacao antes de recuperar takes.";return false;}if(impl&&impl->worker.joinable()){juce::String finishError;stop(finishError);}const auto manifest=directory.getChildFile("recovery.json");if(manifest.getSize()>1024*1024){error="Manifesto de captura excede limite.";return false;}
    const auto data=juce::JSON::parse(manifest.loadFileAsString());if(!data.isObject()||data["format"].toString()!="nova-recording-recovery"||!data["files"].isArray()||data["files"].size()>4096){error="Pasta de captura invalida.";return false;}
    struct Pending {int track;Track::Take take;};std::vector<Pending> pending;
    for(const auto& item:*data["files"].getArray()){
        if(!item.isObject()||!item["track"].isInt()||!item["file"].isString()||!(item["start"].isDouble()||item["start"].isInt())){error="Entrada de recuperacao invalida.";return false;}
        const int index=int(item["track"]);const auto name=item["file"].toString();const auto file=directory.getChildFile(name);const double start=double(item["start"]);
        if(index<0||index>=64||file.getParentDirectory()!=directory||file.getFileName()!=name||!name.startsWith("track-")||!name.endsWithIgnoreCase(".wav")||!std::isfinite(start)||start<0||start>86400){error="Referencia de captura fora da pasta.";return false;}
        // A repeated manifest entry must not reserve another take slot or decode
        // the same source again. Track/file identifies one recovered take.
        if(std::any_of(pending.begin(),pending.end(),[&](const auto& entry){return entry.track==index&&entry.take.file==file;}))continue;
        juce::String readError;auto clip=AudioEngine::loadWav(file,readError);if(!clip)continue;
        if(start+clip->duration()>86400){error="Captura excede timeline de 24 horas.";return false;}
        pending.push_back({index,{0,0,file,start,clip->duration(),clip->sampleRate,clip->samples.getNumChannels()}});
    }
    if(pending.empty()){error="Nenhum prefixo WAV valido recuperavel.";return false;}AudioEngine::MutationGuard guard(engine);
    std::array<int,64> additions{};for(const auto& item:pending){if(item.track<engine.trackCount()){const auto& track=engine.track(item.track);if(track.midiTrack||track.auxiliary){error="Destino de recuperacao exige faixa de audio.";return false;}bool exists=false;for(const auto& take:track.takes)exists|=take.file==item.take.file;if(exists)continue;}
        if(++additions[item.track]+(item.track<engine.trackCount()?int(engine.track(item.track).takes.size()):0)>128){error="Limite de takes atingido; fontes permanecem na pasta.";return false;}}
    const auto group=engine.newRegionId();
    for(auto& item:pending){while(engine.trackCount()<=item.track)if(!engine.addAudioTrack("Recovered "+juce::String(engine.trackCount()+1),error))return false;auto& track=engine.track(item.track);
        if(track.midiTrack||track.auxiliary){error="Destino de recuperacao exige faixa de audio.";return false;}
        bool exists=false;for(const auto& take:track.takes)exists|=take.file==item.take.file;if(exists)continue;
        if(track.takes.size()>=128){error="Limite de takes atingido; fontes permanecem na pasta.";return false;}
        item.take.id=engine.newRegionId();item.take.group=group;const auto id=item.take.id;track.takes.push_back(std::move(item.take));if(!engine.activateTake(item.track,id,error))return false;
    }
    lastStatus="WAV recuperado; fontes originais preservadas";return true;
}
}
