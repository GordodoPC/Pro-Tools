#include "Audio/MixProcessor.h"
#include "Audio/AudioEngine.h"
#include "Session/Session.h"
#include "Midi/MidiData.h"
#include "Midi/MidiIO.h"
#include "Midi/MidiRuntime.h"
#include <iostream>
#include <stdexcept>
namespace {int checks=0;void expect(bool value,const char* text){if(!value)throw std::runtime_error(text);++checks;std::cout<<"PASS: "<<text<<'\n';}}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    expect(argc==4,"instrument module scanner and data paths supplied");juce::String error;juce::File dir(juce::String::fromUTF8(argv[1]));dir.createDirectory();
    juce::ChildProcess scanner;const auto scanOutput=dir.getChildFile("synth-scan.xml");
    expect(scanner.start(juce::StringArray{juce::String::fromUTF8(argv[3]),juce::String::fromUTF8(argv[2]),scanOutput.getFullPathName()},0)&&scanner.waitForProcessToFinish(20000)&&scanner.getExitCode()==0,"isolated scanner probes actual instrument module");
    auto xml=juce::parseXML(scanOutput);expect(xml!=nullptr,"scanner writes instrument metadata");juce::KnownPluginList catalog;catalog.recreateFromXml(*xml);
    expect(catalog.getNumTypes()==1&&catalog.getTypes()[0].isInstrument&&catalog.getTypes()[0].numInputChannels==0&&catalog.getTypes()[0].numOutputChannels==2,"VST3i identified as zero-input stereo instrument");
    auto processor=nova::MixProcessor::loadPlugin(catalog.getTypes()[0],48000,error);
    expect(processor&&processor->plugin&&processor->isInstrument(),"load actual VST3i with MIDI input");
    juce::AudioBuffer<float> audio(2,256);audio.clear();juce::MidiBuffer notes;notes.ensureSize(8192);
    notes.addEvent(juce::MidiMessage::noteOn(16,69,uint8_t(127)),17);notes.addEvent(juce::MidiMessage::noteOff(16,69),150);
    processor->process(audio,256,&notes);bool before=true,after=true;for(int c=0;c<2;++c){for(int s=0;s<17;++s)before&=audio.getSample(c,s)==0;for(int s=150;s<256;++s)after&=audio.getSample(c,s)==0;}
    expect(before&&audio.getMagnitude(17,133)>0.07f,"Note On produces real oscillator audio at its buffer offset");
    expect(after,"Note Off silences instrument at exact buffer offset");
    processor->setParameter(0,0.25f);audio.clear();notes.clear();notes.addEvent(juce::MidiMessage::noteOn(1,69,uint8_t(127)),0);processor->process(audio,256,&notes);
    expect(audio.getMagnitude(0,256)>0.035f&&audio.getMagnitude(0,256)<0.038f,"instrument parameter changes real audio amplitude");
    auto restored=nova::MixProcessor::restore(processor->state(),48000,error);expect(restored&&restored->plugin&&std::abs(restored->parameter(0)-0.25f)<1e-5f,"instrument opaque state restores gain");
    restored->openEditor();restored->closeEditor();expect(restored->plugin->getActiveEditor()==nullptr,"instrument editor closes safely before unload");
    audio.clear();notes.clear();nova::midiPanic(notes);processor->process(audio,256,&notes);expect(audio.getMagnitude(0,256)==0,"all-notes-off prevents stuck voices");
    expect(processor->plugin->getNumPrograms()>=2,"instrument exposes usable programs/presets");processor->plugin->setCurrentProgram(1);notes.clear();notes.addEvent(juce::MidiMessage::noteOn(1,69,uint8_t(127)),0);audio.clear();processor->process(audio,256,&notes);
    expect(std::abs(audio.getSample(0,5)-0.006875f)<0.0002f,"selected triangle preset changes real oscillator waveform");
    auto presetRestored=nova::MixProcessor::restore(processor->state(),48000,error);expect(presetRestored&&presetRestored->plugin&&presetRestored->plugin->getCurrentProgram()==1,"opaque instrument preset state survives recovery");
    notes.clear();notes.addEvent(juce::MidiMessage::controllerEvent(1,7,0),0);notes.addEvent(juce::MidiMessage::noteOn(1,69,uint8_t(127)),0);audio.clear();processor->process(audio,256,&notes);expect(audio.getMagnitude(0,256)==0,"MIDI Control Change reaches instrument and affects real audio");
    auto missing=processor->state();auto desc=processor->description;desc.fileOrIdentifier=dir.getChildFile("missing-synth.vst3").getFullPathName();missing.getDynamicObject()->setProperty("description",desc.createXml()->toString());
    auto absent=nova::MixProcessor::restore(missing,48000,error);expect(absent&&!absent->plugin&&absent->isInstrument(),"missing instrument preserves role and opaque state");
    audio.clear();notes.clear();notes.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),0);absent->process(audio,256,&notes);expect(audio.getMagnitude(0,256)==0,"missing instrument does not invent audio");
    nova::AudioEngine engine;engine.setOutputRate(48000);engine.setSmoothingTime(0);expect(engine.addMidiTrack("MIDI",error),"create musical track in existing mixer graph");
    engine.track(0).instrument=nova::MixProcessor::loadPlugin(catalog.getTypes()[0],48000,error);engine.track(0).midiClips[0].notes={{1,1,10,69,127,16}};
    expect(engine.rebuildMidi(0,error),"compile track notes off callback");engine.prepareProcessors();engine.play();juce::AudioBuffer<float> song(2,600);engine.render(song.getArrayOfWritePointers(),2,600);
    expect(song.getMagnitude(0,25)==0&&song.getMagnitude(25,250)>0.07f&&song.getMagnitude(275,325)==0,"musical track sends exact-timed MIDI and routes real instrument audio to Master");
    engine.seek(nova::ticksToSeconds(5,120));engine.render(song.getArrayOfWritePointers(),2,64);expect(song.getMagnitude(0,64)>0.01f,"seek into sustained note rehydrates its voice");
    engine.stop();engine.setTempo(60);engine.play();engine.render(song.getArrayOfWritePointers(),2,600);
    expect(song.getMagnitude(0,50)==0&&song.getMagnitude(50,500)>0.07f&&song.getMagnitude(550,50)==0,"BPM change scales musical note timing without changing its pitch");
    auto* playhead=engine.track(0).instrument->plugin->getPlayHead();expect(playhead&&playhead->getPosition()->getBpm()==60,"instrument receives host tempo and transport context");
    engine.stop();engine.setTempo(120);engine.setLoop(0,300.0/48000,true);engine.play();juce::AudioBuffer<float> looping(2,900);engine.render(looping.getArrayOfWritePointers(),2,900);
    bool repeated=true;for(int repeat=0;repeat<3;++repeat)repeated&=looping.getMagnitude(repeat*300,25)==0&&looping.getMagnitude(repeat*300+25,250)>0.07f&&looping.getMagnitude(repeat*300+275,25)==0;
    expect(repeated,"MIDI loop boundaries retrigger and release voices without stuck notes");
    const auto session=dir.getChildFile("midi-v4.novadaw");expect(nova::Session::save(session,engine,error),"save version-4 MIDI session with opaque instrument state");
    nova::AudioEngine recovered;recovered.setOutputRate(48000);recovered.setSmoothingTime(0);expect(nova::Session::load(session,recovered,error)&&recovered.track(0).midiTrack&&recovered.track(0).midiClips[0].notes.size()==1&&recovered.track(0).instrument->plugin,"reopen MIDI notes clips loop and real instrument");
    recovered.play();recovered.render(looping.getArrayOfWritePointers(),2,900);expect(looping.getMagnitude(25,250)>0.07f,"restored MIDI session produces real instrument audio");
    recovered.stop();recovered.track(0).midiClips.clear();recovered.rebuildMidi(0,error);
    recovered.postMidi(0,juce::MidiMessage::noteOn(1,69,uint8_t(127)),17);recovered.postMidi(0,juce::MidiMessage::noteOff(1,69),150);
    recovered.render(song.getArrayOfWritePointers(),2,256);expect(song.getMagnitude(0,17)==0&&song.getMagnitude(17,133)>0.07f&&song.getMagnitude(150,106)==0&&recovered.position()==0,"live MIDI sounds at offsets while stopped without advancing transport");
    recovered.midiRecording=true;recovered.track(0).midiArm=true;recovered.play();recovered.postMidi(0,juce::MidiMessage::noteOn(1,60,uint8_t(100)),17);recovered.postMidi(0,juce::MidiMessage::noteOff(1,60),150);
    recovered.render(song.getArrayOfWritePointers(),2,256);nova::CapturedMidiEvent captured;std::vector<int64_t> captureTicks;while(recovered.popCapturedMidi(captured))captureTicks.push_back(captured.tick);
    expect(captureTicks==std::vector<int64_t>{1,6},"live input capture preserves note offsets rounded to PPQ ticks");recovered.stop();
    recovered.setMidiInput(0,"fixture-input");recovered.dispatchMidiDevice(nova::midiDeviceToken("different-device"),juce::MidiMessage::noteOn(1,69,uint8_t(127)));recovered.render(song.getArrayOfWritePointers(),2,64);
    expect(song.getMagnitude(0,64)==0,"unselected MIDI device is not routed to instrument");
    recovered.dispatchMidiDevice(nova::midiDeviceToken("fixture-input"),juce::MidiMessage::noteOn(1,69,uint8_t(127)));recovered.render(song.getArrayOfWritePointers(),2,64);
    expect(song.getMagnitude(0,64)>0.01f,"selected simulated MIDI device routes through bounded ingress");recovered.stop();
    expect(nova::Session::load(session,recovered,error),"restore MIDI fixture after live input test");
    recovered.stop();recovered.track(0).midiClips.clear();recovered.rebuildMidi(0,error);recovered.track(0).midiArm=true;
    nova::MidiRecorder recorder(recovered);expect(recorder.start(dir.getChildFile("midi-recordings"),error),"start armed MIDI recorder without simulating a physical device");
    recovered.postMidi(0,juce::MidiMessage::noteOn(16,60,uint8_t(100)),17);recovered.postMidi(0,juce::MidiMessage::noteOff(16,60),150);recovered.render(song.getArrayOfWritePointers(),2,256);recorder.service();
    expect(recorder.stop(error)&&recovered.track(0).midiClips.size()==1&&recovered.track(0).midiClips[0].notes.size()==1,"simulated MIDI input records a non-destructive musical take");
    const auto recorded=recovered.track(0).midiClips[0].notes[0];expect(recorded.tick==1&&recorded.length==5&&recorded.velocity==100&&recorded.channel==16,"recorded note retains PPQ time length velocity and channel");
    const auto backups=dir.getChildFile("midi-recordings").findChildFiles(juce::File::findFiles,false,"*.mid");expect(!backups.isEmpty(),"MIDI recording creates a separate recovery source file");
    const auto recordedSession=dir.getChildFile("recorded-midi.novadaw");expect(nova::Session::save(recordedSession,recovered,error),"save recorded MIDI data");nova::AudioEngine reopened;reopened.setOutputRate(48000);
    expect(nova::Session::load(recordedSession,reopened,error)&&reopened.track(0).midiClips[0].notes[0].velocity==100,"reopen session preserves MIDI recording");
    expect(nova::Session::load(session,recovered,error),"restore MIDI loop fixture after recorder test");
    nova::AudioEngine timestamped;timestamped.setOutputRate(48000);timestamped.setSmoothingTime(0);timestamped.addMidiTrack("Timestamp",error);timestamped.track(0).instrument=nova::MixProcessor::loadPlugin(catalog.getTypes()[0],48000,error);timestamped.setMidiInput(0,"timed-device");timestamped.prepareProcessors();
    const double clock=juce::Time::getMillisecondCounterHiRes()*0.001;timestamped.renderAtTime(song.getArrayOfWritePointers(),2,256,clock);
    auto timedOn=juce::MidiMessage::noteOn(1,69,uint8_t(127));timedOn.setTimeStamp(clock+17.0/48000);auto timedOff=juce::MidiMessage::noteOff(1,69);timedOff.setTimeStamp(clock+150.0/48000);
    timestamped.dispatchMidiDevice(nova::midiDeviceToken("timed-device"),timedOn);timestamped.dispatchMidiDevice(nova::midiDeviceToken("timed-device"),timedOff);
    juce::AudioBuffer<float> timed(2,192);for(int offset=0;offset<192;offset+=64){float* pointers[]{timed.getWritePointer(0)+offset,timed.getWritePointer(1)+offset};timestamped.renderAtTime(pointers,2,64,clock+(256+offset)/48000.0);}
    expect(timed.getMagnitude(0,17)==0&&timed.getMagnitude(17,133)>0.07f&&timed.getMagnitude(150,42)==0,"timestamped simulated device preserves intra-block timing and future Note Off across buffer-size change");
    auto invalid=juce::JSON::parse(session.loadFileAsString());invalid["tracks"].getArray()->getReference(0)["midiClips"].getArray()->getReference(0).getDynamicObject()->setProperty("length",int64_t(-1));
    const auto badSession=dir.getChildFile("invalid-midi.novadaw");badSession.replaceWithText(juce::JSON::toString(invalid));expect(!nova::Session::load(badSession,recovered,error)&&recovered.track(0).midiClips[0].notes.size()==1,"invalid MIDI metadata preserves currently open session");
    nova::AudioEngine click;click.setOutputRate(48000);click.addMidiTrack("Clock",error);click.metronomeEnabled=true;click.play();click.render(song.getArrayOfWritePointers(),2,600);expect(song.getMagnitude(0,600)>0.05f,"metronome generates actual Master audio");
    std::cout<<checks<<" MIDI instrument checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
