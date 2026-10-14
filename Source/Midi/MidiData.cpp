#include "MidiData.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace nova {
namespace {
constexpr int64_t maximumTick=414720000; // 24 hours at 300 BPM.
bool integer(const juce::var& v){return v.isInt()||v.isInt64();}
bool validEvent(const MidiEvent& e){
    const auto status=e.bytes[0]&0xf0;
    return e.tick>=0&&e.tick<=maximumTick && e.bytes[0]>=0x80&&e.bytes[0]<0xf0
        && e.size==(status==0xc0||status==0xd0?2:3)
        && e.bytes[1]<128&&(e.size==2||e.bytes[2]<128);
}
void append(std::vector<MidiEvent>& events,int64_t tick,int status,int a,int b=0,int size=3){
    events.push_back({tick,{static_cast<uint8_t>(status),static_cast<uint8_t>(a),static_cast<uint8_t>(b)},static_cast<uint8_t>(size)});
}
}
double ticksToSeconds(int64_t ticks,double bpm)noexcept{return ticks*60.0/(bpm*midiPpq);}
int64_t secondsToTicks(double seconds,double bpm)noexcept{return std::llround(seconds*bpm*midiPpq/60.0);}
int64_t ticksToSamples(int64_t ticks,double bpm,double rate)noexcept{return std::llround(ticksToSeconds(ticks,bpm)*rate);}
bool compileMidi(const std::vector<MidiClip>& clips,std::vector<MidiEvent>& destination,juce::String& error){
    if(clips.size()>128){error="Limite de 128 clips MIDI.";return false;}
    std::vector<MidiEvent> compiled;
    for(const auto& clip:clips){
        if(clip.start<0||clip.offset<0||clip.length<=0||clip.start>maximumTick-clip.length||clip.offset>maximumTick-clip.length
            ||clip.notes.size()>4096||clip.controls.size()>4096){error="Clip MIDI fora dos limites.";return false;}
        const auto sourceEnd=clip.offset+clip.length;
        for(const auto& note:clip.notes){
            if(note.tick<0||note.length<=0||note.tick>maximumTick-note.length||note.pitch<0||note.pitch>127
                ||note.velocity<1||note.velocity>127||note.channel<1||note.channel>16){error="Nota MIDI invalida.";return false;}
            const auto begin=std::max(note.tick,clip.offset),end=std::min(note.tick+note.length,sourceEnd);
            if(begin>=end)continue;
            append(compiled,clip.start+begin-clip.offset,0x90+note.channel-1,note.pitch,note.velocity);
            append(compiled,clip.start+end-clip.offset,0x80+note.channel-1,note.pitch);
        }
        for(auto event:clip.controls){if(!validEvent(event)){error="Evento MIDI invalido.";return false;}
            if(event.tick<clip.offset||event.tick>=sourceEnd)continue;event.tick+=clip.start-clip.offset;compiled.push_back(event);}
        if(compiled.size()>65536){error="Limite de 65536 eventos MIDI por faixa.";return false;}
    }
    std::stable_sort(compiled.begin(),compiled.end(),[](const auto& a,const auto& b){
        if(a.tick!=b.tick)return a.tick<b.tick;
        // Release old notes before retriggering a pitch at the same musical instant.
        return (a.bytes[0]&0xf0)==0x80 && (b.bytes[0]&0xf0)!=0x80;
    });
    destination=std::move(compiled);return true;
}
void midiPanic(juce::MidiBuffer& destination,int sample)noexcept{
    for(int channel=0;channel<16;++channel){const uint8_t bytes[]{static_cast<uint8_t>(0xb0+channel),123,0};destination.addEvent(bytes,3,sample);}
}
bool scheduleMidi(const std::vector<MidiEvent>& events,double seconds,double bpm,double rate,int frames,juce::MidiBuffer& destination)noexcept{
    destination.clear();if(frames<=0||!std::isfinite(seconds)||!std::isfinite(bpm)||!std::isfinite(rate)||bpm<30||bpm>300||rate<100)return true;
    const auto begin=std::llround(seconds*rate),end=begin+frames;
    auto next=std::lower_bound(events.begin(),events.end(),begin,[&](const MidiEvent& event,int64_t sample){return ticksToSamples(event.tick,bpm,rate)<sample;});
    int added=0;
    for(;next!=events.end();++next){const auto sample=ticksToSamples(next->tick,bpm,rate);if(sample>=end)break;
        if(++added>maximumMidiEventsPerBlock){destination.clear();midiPanic(destination);return false;}
        destination.addEvent(next->bytes.data(),next->size,static_cast<int>(sample-begin));
    }
    return true;
}
juce::var midiClipState(const MidiClip& clip){
    auto o=std::make_unique<juce::DynamicObject>();o->setProperty("start",clip.start);o->setProperty("offset",clip.offset);o->setProperty("length",clip.length);o->setProperty("name",clip.name);
    juce::Array<juce::var> notes;for(const auto& note:clip.notes){auto n=std::make_unique<juce::DynamicObject>();n->setProperty("tick",note.tick);n->setProperty("length",note.length);
        n->setProperty("pitch",note.pitch);n->setProperty("velocity",note.velocity);n->setProperty("channel",note.channel);notes.add(juce::var(n.release()));}o->setProperty("notes",notes);
    juce::Array<juce::var> controls;for(const auto& event:clip.controls){auto e=std::make_unique<juce::DynamicObject>();e->setProperty("tick",event.tick);
        e->setProperty("status",int(event.bytes[0]));e->setProperty("data1",int(event.bytes[1]));e->setProperty("data2",int(event.bytes[2]));e->setProperty("size",int(event.size));controls.add(juce::var(e.release()));}o->setProperty("controls",controls);
    return juce::var(o.release());
}
bool readMidiClip(const juce::var& v,MidiClip& clip,juce::String& error){
    if(!v.isObject()||!integer(v["start"])||!integer(v["offset"])||!integer(v["length"])||!v["name"].isString()
        ||!v["notes"].isArray()||!v["controls"].isArray()||v["notes"].size()>4096||v["controls"].size()>4096){error="Dados MIDI invalidos.";return false;}
    MidiClip candidate;candidate.start=int64_t(v["start"]);candidate.offset=int64_t(v["offset"]);candidate.length=int64_t(v["length"]);candidate.name=v["name"].toString().substring(0,80);
    for(const auto& n:*v["notes"].getArray()){
        if(!n.isObject()||!integer(n["tick"])||!integer(n["length"])||!n["pitch"].isInt()||!n["velocity"].isInt()||!n["channel"].isInt()){error="Nota MIDI invalida.";return false;}
        candidate.notes.push_back({0,int64_t(n["tick"]),int64_t(n["length"]),int(n["pitch"]),int(n["velocity"]),int(n["channel"])});
    }
    for(const auto& e:*v["controls"].getArray()){
        if(!e.isObject()||!integer(e["tick"])||!e["status"].isInt()||!e["data1"].isInt()||!e["data2"].isInt()||!e["size"].isInt()
            ||int(e["status"])<128||int(e["status"])>239||int(e["data1"])<0||int(e["data1"])>127||int(e["data2"])<0||int(e["data2"])>127
            ||(int(e["size"])!=2&&int(e["size"])!=3)){error="Controle MIDI invalido.";return false;}
        candidate.controls.push_back({int64_t(e["tick"]),{uint8_t(int(e["status"])),uint8_t(int(e["data1"])),uint8_t(int(e["data2"]))},uint8_t(int(e["size"]))});
    }
    std::vector<MidiEvent> checked;if(!compileMidi({candidate},checked,error))return false;clip=std::move(candidate);return true;
}
bool importSmf(const juce::File& file,MidiClip& clip,juce::String& error){
    if(file.getSize()>16*1024*1024){error="MIDI excede 16 MiB.";return false;}
    auto input=file.createInputStream();juce::MidiFile midi;if(!input||!midi.readFrom(*input)||midi.getTimeFormat()<=0){error="Standard MIDI File invalido; SMPTE nao suportado.";return false;}
    MidiClip candidate;candidate.name=file.getFileNameWithoutExtension();int64_t end=midiPpq*4;
    for(int track=0;track<midi.getNumTracks();++track){auto sequence=*midi.getTrack(track);sequence.updateMatchedPairs();
        for(int i=0;i<sequence.getNumEvents();++i){const auto* holder=sequence.getEventPointer(i);const auto& message=holder->message;
            const auto tick=std::llround(message.getTimeStamp()*midiPpq/midi.getTimeFormat());
            if(message.isNoteOn()){
                const auto* off=holder->noteOffObject;if(!off){error="Note On MIDI sem Note Off.";return false;}
                const auto length=std::max<int64_t>(1,std::llround((off->message.getTimeStamp()-message.getTimeStamp())*midiPpq/midi.getTimeFormat()));
                candidate.notes.push_back({0,tick,length,message.getNoteNumber(),int(message.getVelocity()),message.getChannel()});end=std::max(end,tick+length);
            }else if(!message.isNoteOff()&&(message.isController()||message.isProgramChange()||message.isPitchWheel()||message.isAftertouch()||message.isChannelPressure())){
                MidiEvent event;event.tick=tick;event.size=uint8_t(message.getRawDataSize());std::copy_n(message.getRawData(),event.size,event.bytes.begin());candidate.controls.push_back(event);end=std::max(end,tick+1);
            }
        }
    }
    candidate.length=end;std::vector<MidiEvent> checked;if(!compileMidi({candidate},checked,error))return false;clip=std::move(candidate);return true;
}
bool exportSmf(const juce::File& file,const std::vector<MidiClip>& clips,double bpm,int beats,int unit,juce::String& error){
    if(!std::isfinite(bpm)||bpm<30||bpm>300||beats<1||beats>12||(unit!=4&&unit!=8)){error="Tempo/compasso MIDI invalido.";return false;}
    std::vector<MidiEvent> events;if(!compileMidi(clips,events,error))return false;
    juce::MidiMessageSequence sequence;sequence.addEvent(juce::MidiMessage::tempoMetaEvent(int(60000000.0/bpm)));
    sequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(beats,unit));
    for(const auto& event:events){juce::MidiMessage message(event.bytes.data(),event.size,double(event.tick));sequence.addEvent(message);}
    sequence.updateMatchedPairs();juce::MidiFile midi;midi.setTicksPerQuarterNote(midiPpq);midi.addTrack(sequence);
    juce::TemporaryFile temporary(file);auto output=temporary.getFile().createOutputStream();if(!output||!midi.writeTo(*output)){error="Falha ao escrever MIDI.";return false;}
    output->flush();output.reset();if(!temporary.overwriteTargetFileWithTemporary()){error="Falha ao publicar MIDI.";return false;}return true;
}
}
