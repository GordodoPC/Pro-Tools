#include "MidiRuntime.h"
#include "MidiIO.h"
#include "Audio/AudioEngine.h"
namespace nova {
MidiPort::MidiPort(AudioEngine& owner,const juce::String& id):identifier(id),engine(owner),token(midiDeviceToken(id)){
    input=juce::MidiInput::openDevice(id,this);if(input)input->start();
}
MidiPort::~MidiPort(){if(input)input->stop();}
void MidiPort::handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage& message){engine.dispatchMidiDevice(token,message);}
MidiRecorder::MidiRecorder(AudioEngine& owner):engine(owner),held(std::make_unique<HeldNotes>()){}
bool MidiRecorder::start(const juce::File& recoveryDirectory,juce::String& error){
    if(active||unsaved){error=active?"MIDI ja esta gravando.":"Captura MIDI anterior tem arquivo de recuperacao pendente.";return false;}
    bool any=false;for(int i=0;i<engine.trackCount();++i)any|=engine.track(i).midiTrack&&engine.track(i).midiArm;
    if(!any){error="Arme uma faixa MIDI antes de gravar.";return false;}
    if(!recoveryDirectory.createDirectory().wasOk()){error="Pasta de recuperacao MIDI indisponivel.";return false;}
    AudioEngine::MutationGuard guard(engine);folder=recoveryDirectory;generation=engine.midiGeneration();captureDropsAtStart=engine.midiCaptureDrops();tempo=engine.bpm.load();startTick=secondsToTicks(engine.position(),tempo);lastTick=startTick;
    held=std::make_unique<HeldNotes>();armed={};interrupted=false;
    CapturedMidiEvent old;for(int i=0;i<2048&&engine.popCapturedMidi(old);++i){}
    for(int i=0;i<64;++i){takes[i]=MidiClip{};takes[i].start=startTick;takes[i].length=1;
        if(i<engine.trackCount()&&engine.track(i).midiTrack&&engine.track(i).midiArm){armed[i]=true;takes[i].name=engine.track(i).name+" take";}}
    restoreLoop=engine.loopEnabled.load();oldLoopStart=engine.loopStart.load();oldLoopEnd=engine.loopEnd.load();engine.loopEnabled=false;
    engine.midiRecordEndTick=startTick;engine.midiRecording=true;engine.play();active=true;lastStatus="Gravando MIDI; recuperacao em "+folder.getFullPathName();return true;
}
void MidiRecorder::finishNote(int track,int channel,int pitch,int64_t end){
    auto& note=(*held)[track][channel*128+pitch];if(!note.active)return;auto& take=takes[track];
    if(take.notes.size()>=4096){interrupted=true;engine.midiRecording=false;note.active=false;return;}
    take.notes.push_back({0,std::max<int64_t>(0,note.tick-startTick),std::max<int64_t>(1,end-note.tick),pitch,note.velocity,channel+1});note.active=false;
}
void MidiRecorder::drain(){
    CapturedMidiEvent event;for(int received=0;received<2048&&engine.popCapturedMidi(event);++received){
        if(event.generation!=generation||event.track<0||event.track>=64||!armed[event.track]||event.tick<startTick)continue;
        lastTick=std::max(lastTick,event.tick);const int channel=event.bytes[0]&15,status=event.bytes[0]&0xf0,pitch=event.bytes[1];
        if(status==0x90&&event.bytes[2]!=0){finishNote(event.track,channel,pitch,event.tick);(*held)[event.track][channel*128+pitch]={event.tick,event.bytes[2],true};}
        else if(status==0x80||status==0x90)finishNote(event.track,channel,pitch,event.tick);
        else {auto& controls=takes[event.track].controls;if(controls.size()>=4096){interrupted=true;engine.midiRecording=false;continue;}
            controls.push_back({event.tick-startTick,event.bytes,event.size});
            if(status==0xb0&&(event.bytes[1]==123||event.bytes[1]==120))for(int note=0;note<128;++note)finishNote(event.track,channel,note,event.tick);}
    }
}
bool MidiRecorder::stop(juce::String& error){
    if(!active)return !unsaved;
    AudioEngine::MutationGuard guard(engine);engine.midiRecording=false;drain();const bool sameSession=generation==engine.midiGeneration();
    const auto end=std::max(lastTick+1,engine.midiRecordEndTick.load());for(int i=0;i<64;++i)if(armed[i])for(int channel=0;channel<16;++channel)for(int pitch=0;pitch<128;++pitch)finishNote(i,channel,pitch,end);
    bool ok=true;int completed=0;
    for(int i=0;i<64;++i)if(armed[i]&&(!takes[i].notes.empty()||!takes[i].controls.empty())){
        auto& take=takes[i];take.length=std::max<int64_t>(1,end-startTick+1);for(const auto& note:take.notes)take.length=std::max(take.length,note.tick+note.length);for(const auto& control:take.controls)take.length=std::max(take.length,control.tick+1);
        auto backup=take;backup.start=0;const auto output=folder.getNonexistentChildFile("MIDI-track-"+juce::String(i+1),".mid");juce::String saveError;
        if(!exportSmf(output,{backup},tempo,engine.beatsPerBar,engine.beatUnit,saveError)){ok=false;unsaved=true;error=saveError;}
        if(sameSession&&i<engine.trackCount()&&engine.track(i).midiTrack){auto& track=engine.track(i);
            if(track.midiClips.size()>=128){ok=false;error="Take MIDI preservado no arquivo; limite de clips atingido.";continue;}
            take.id=engine.newRegionId();for(auto& note:take.notes)note.id=engine.newRegionId();track.midiClips.push_back(take);
            if(!engine.rebuildMidi(i,saveError)){track.midiClips.pop_back();ok=false;error=saveError;}else ++completed;
        }
    }
    if(sameSession&&restoreLoop)engine.setLoop(oldLoopStart,oldLoopEnd,true);
    active=false;if(sameSession)engine.pause();lastStatus=ok?"MIDI: "+juce::String(completed)+" takes, fontes .mid preservadas":"MIDI: captura parcial; "+error;
    if(interrupted||engine.midiCaptureDrops()>captureDropsAtStart){error="Captura MIDI interrompida/overflow; material recebido foi preservado.";lastStatus=error;return false;}return ok;
}
bool MidiRecorder::saveRecovery(const juce::File& directory,juce::String& error){
    if(active){error="Pare a captura antes de recuperar MIDI.";return false;}if(!directory.createDirectory().wasOk()){error="Pasta MIDI indisponivel.";return false;}
    for(auto take:takes)if(!take.notes.empty()||!take.controls.empty()){take.start=0;if(!exportSmf(directory.getNonexistentChildFile("MIDI-recovery",".mid"),{take},tempo,engine.beatsPerBar,engine.beatUnit,error))return false;}
    unsaved=false;lastStatus="MIDI recebido preservado em "+directory.getFullPathName();return true;
}
void MidiRecorder::service(){if(!active)return;drain();if(!engine.midiRecording||!engine.isPlaying()||generation!=engine.midiGeneration()||engine.bpm.load()!=tempo){juce::String error;stop(error);}}
}
