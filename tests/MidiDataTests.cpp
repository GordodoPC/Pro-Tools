#include "Midi/MidiData.h"
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
    std::cout<<checks<<" MIDI data checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
