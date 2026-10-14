#include "Midi/MidiData.h"
#include "Midi/MidiEditModel.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
namespace {bool measuring=false;int allocations=0,checks=0;}
void* operator new(size_t size){if(measuring)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}
namespace {void expect(bool value,const char* label){if(!value)throw std::runtime_error(label);++checks;std::cout<<"PASS: "<<label<<'\n';}}
int main(int argc,char** argv){try{
    expect(argc==2,"MIDI test output directory supplied");const juce::File dir(juce::String::fromUTF8(argv[1]));dir.createDirectory();
    expect(nova::ticksToSamples(960,120,48000)==24000&&nova::ticksToSamples(960,60,48000)==48000,"quarter note sample duration follows BPM");
    expect(nova::secondsToTicks(1,120)==1920,"seconds convert to PPQ ticks");
    nova::MidiClip clip;clip.notes={{1,1,10,60,100,16}};clip.controls={{0,{0xb0,7,64},3},{12,{0xc0,5,0},2},{13,{0xe0,0,64},3}};
    std::vector<nova::MidiEvent> compiled;juce::String error;expect(nova::compileMidi({clip},compiled,error)&&compiled.size()==5,"compile note release CC Program Change and Pitch Bend");
    juce::MidiBuffer events;events.ensureSize(8192);nova::scheduleMidi(compiled,0,120,48000,256,events);
    std::vector<int> offsets;for(auto event:events)offsets.push_back(event.samplePosition);
    expect(offsets==std::vector<int>{0,25},"events have exact intra-block sample offsets");
    nova::scheduleMidi(compiled,256.0/48000,120,48000,256,events);offsets.clear();for(auto event:events)offsets.push_back(event.samplePosition);
    expect(offsets==std::vector<int>{19,44,69},"note off and controls cross buffer boundary without duplicates");
    bool noAllocations=true;measuring=true;for(int i=0;i<1000;++i)noAllocations&=nova::scheduleMidi(compiled,0,120,48000,256,events);measuring=false;
    expect(noAllocations&&allocations==0,"MIDI scheduling into prepared buffer does not allocate C++ storage");
    auto state=nova::midiClipState(clip);nova::MidiClip restored;expect(nova::readMidiClip(state,restored,error)&&restored.notes[0].channel==16,"MIDI notes and channel persist");
    expect(nova::exportSmf(dir.getChildFile("roundtrip.mid"),{clip},120,4,4,error),"write valid Standard MIDI File atomically");
    nova::MidiClip imported;expect(nova::importSmf(dir.getChildFile("roundtrip.mid"),imported,error)&&imported.notes.size()==1&&imported.controls.size()==3,"SMF note and channel-message roundtrip");
    expect(imported.notes[0].tick==1&&imported.notes[0].length==10&&imported.notes[0].velocity==100&&imported.notes[0].channel==16,"SMF preserves musical duration velocity and channel");
    auto bad=nova::midiClipState(clip);bad["controls"].getArray()->getReference(0).getDynamicObject()->setProperty("size",258);
    expect(!nova::readMidiClip(bad,restored,error)&&restored.notes[0].channel==16,"invalid event size rejected without replacing valid MIDI clip");
    std::vector<nova::MidiEvent> dense(257,{0,{0x90,60,100},3});expect(!nova::scheduleMidi(dense,0,120,48000,64,events)&&events.getNumEvents()==16,"event overflow replaces oversized block with channel panic");
    auto trim=clip;trim.start=960;trim.offset=5;trim.length=5;trim.controls.clear();expect(nova::compileMidi({trim},compiled,error)&&compiled[0].tick==960&&compiled[1].tick==965,"non-destructive trim clips note edges in musical time");
    nova::AudioEngine engine;engine.addMidiTrack("Edit",error);nova::MidiEditModel edit(engine,0);edit.snap=false;
    expect(edit.addNote(73,60,200,100,1)&&edit.clip()->notes[0].tick==73,"piano model inserts unsnapped note with exact musical position");
    expect(edit.moveSelected(24,2)&&edit.resizeSelected(40)&&edit.clip()->notes[0].tick==97&&edit.clip()->notes[0].pitch==62&&edit.clip()->notes[0].length==240,"piano move and resize alter actual MIDI event data");
    expect(edit.setVelocity(64)&&edit.setChannel(16)&&edit.clip()->notes[0].velocity==64&&edit.clip()->notes[0].channel==16,"piano velocity and channel affect persisted notes");
    edit.addNote(960,64,960,100,1);edit.selectAll();expect(edit.copy()&&edit.paste(1920)&&edit.clip()->notes.size()==4,"multi-note copy paste preserves relative musical spacing");
    expect(edit.quantizeSelected()&&edit.clip()->notes.back().tick==2880,"quantization moves selected notes onto chosen grid");
    expect(edit.eraseSelected()&&edit.clip()->notes.size()==2&&edit.undo()&&edit.clip()->notes.size()==4&&edit.redo()&&edit.clip()->notes.size()==2,"piano delete undo redo restore selected musical data");
    expect(!edit.setClipTiming(400000000,20000000,0)&&edit.clip()->start==0&&edit.clip()->notes.size()==2,"invalid MIDI edit rolls back to prior clip and compiled events");
    expect(edit.duplicateClip()&&engine.track(0).midiClips.size()==2&&edit.undo()&&engine.track(0).midiClips.size()==1,"clip duplication is non-destructive and undoable");
    std::cout<<checks<<" MIDI data checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
