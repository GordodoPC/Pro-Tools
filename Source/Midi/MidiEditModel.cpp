#include "MidiEditModel.h"
#include <algorithm>
#include <cmath>
namespace nova {
MidiEditModel::MidiEditModel(AudioEngine& owner,int track):engine(owner),trackIndex(track),generation(owner.midiGeneration()){}
bool MidiEditModel::valid()const{return generation==engine.midiGeneration()&&trackIndex>=0&&trackIndex<engine.trackCount()&&engine.track(trackIndex).midiTrack;}
MidiClip* MidiEditModel::clip(){if(!valid())return nullptr;auto& clips=engine.track(trackIndex).midiClips;return clipIndex>=0&&clipIndex<int(clips.size())?&clips[clipIndex]:nullptr;}
const MidiClip* MidiEditModel::clip()const{return const_cast<MidiEditModel*>(this)->clip();}
int64_t MidiEditModel::quantize(int64_t tick)const{return std::max<int64_t>(0,snap&&grid>0?std::llround(double(tick)/grid)*grid:tick);}
void MidiEditModel::select(uint64_t id,bool extend){if(!extend)selected.clear();if(id){if(extend&&selected.contains(id))selected.erase(id);else selected.insert(id);}}
void MidiEditModel::selectAll(){selected.clear();if(auto* current=clip())for(const auto& note:current->notes)selected.insert(note.id);}
MidiEditModel::State MidiEditModel::capture()const{return {valid()?engine.track(trackIndex).midiClips:std::vector<MidiClip>{},clipIndex,selected};}
void MidiEditModel::restore(const State& state){engine.track(trackIndex).midiClips=state.clips;clipIndex=state.clipIndex;selected=state.selected;engine.rebuildMidi(trackIndex,lastError);}
void MidiEditModel::limitHistory(){
    auto size=[](const State& state){size_t bytes=state.clips.size()*sizeof(MidiClip);for(const auto& clip:state.clips)bytes+=clip.notes.size()*sizeof(MidiNote)+clip.controls.size()*sizeof(MidiEvent);return bytes;};
    size_t bytes=0;for(const auto& state:past)bytes+=size(state);for(const auto& state:future)bytes+=size(state);
    while(!past.empty()&&(past.size()>64||bytes>64*1024*1024)){bytes-=size(past.front());past.erase(past.begin());}
    while(!future.empty()&&(future.size()>64||bytes>64*1024*1024)){bytes-=size(future.front());future.erase(future.begin());}
}
bool MidiEditModel::edit(const std::function<bool()>& change){
    if(!valid())return false;AudioEngine::MutationGuard guard(engine);auto before=capture();
    if(!change())return false;
    if(!engine.rebuildMidi(trackIndex,lastError)){restore(before);return false;}
    past.push_back(std::move(before));future.clear();limitHistory();return true;
}
void MidiEditModel::assignIds(MidiClip& clip){clip.id=engine.newRegionId();for(auto& note:clip.notes)note.id=engine.newRegionId();}
bool MidiEditModel::addNote(int64_t tick,int pitch,int64_t length,int velocity,int channel){
    return edit([&]{auto* current=clip();if(!current||current->notes.size()>=4096||pitch<0||pitch>127||velocity<1||velocity>127||channel<1||channel>16||length<=0)return false;
        tick=quantize(tick);const auto id=engine.newRegionId();current->notes.push_back({id,tick,length,pitch,velocity,channel});current->length=std::max(current->length,tick+length-current->offset);selected={id};return true;});
}
bool MidiEditModel::moveSelected(int64_t delta,int semitones){
    return edit([&]{auto* current=clip();if(!current||selected.empty()||(delta==0&&semitones==0))return false;
        bool found=false;for(const auto& note:current->notes)if(selected.contains(note.id)){found=true;if(note.tick+delta<0||note.pitch+semitones<0||note.pitch+semitones>127)return false;}
        if(!found)return false;for(auto& note:current->notes)if(selected.contains(note.id)){note.tick+=delta;note.pitch+=semitones;current->length=std::max(current->length,note.tick+note.length-current->offset);}return true;});
}
bool MidiEditModel::resizeSelected(int64_t delta){
    return edit([&]{auto* current=clip();if(!current||selected.empty()||delta==0)return false;bool changed=false;
        for(auto& note:current->notes)if(selected.contains(note.id)){note.length=std::max<int64_t>(1,note.length+delta);current->length=std::max(current->length,note.tick+note.length-current->offset);changed=true;}return changed;});
}
bool MidiEditModel::eraseSelected(){return edit([&]{auto* current=clip();if(!current||selected.empty())return false;const auto old=current->notes.size();std::erase_if(current->notes,[&](const auto& note){return selected.contains(note.id);});selected.clear();return old!=current->notes.size();});}
bool MidiEditModel::copy(){clipboard.clear();if(auto* current=clip())for(const auto& note:current->notes)if(selected.contains(note.id))clipboard.push_back(note);
    if(clipboard.empty())return false;const auto first=std::min_element(clipboard.begin(),clipboard.end(),[](const auto& a,const auto& b){return a.tick<b.tick;})->tick;for(auto& note:clipboard)note.tick-=first;return true;}
bool MidiEditModel::paste(int64_t tick){return edit([&]{auto* current=clip();if(!current||clipboard.empty()||current->notes.size()+clipboard.size()>4096)return false;selected.clear();tick=quantize(tick);
    for(auto note:clipboard){note.id=engine.newRegionId();note.tick+=tick;selected.insert(note.id);current->length=std::max(current->length,note.tick+note.length-current->offset);current->notes.push_back(note);}return true;});}
bool MidiEditModel::quantizeSelected(){return edit([&]{auto* current=clip();if(!current||selected.empty()||grid<=0)return false;bool changed=false;
    for(auto& note:current->notes)if(selected.contains(note.id)){const auto next=std::max<int64_t>(0,std::llround(double(note.tick)/grid)*grid);changed|=next!=note.tick;note.tick=next;}return changed;});}
bool MidiEditModel::setVelocity(int value){return edit([&]{auto* current=clip();if(!current||value<1||value>127)return false;bool changed=false;for(auto& note:current->notes)if(selected.contains(note.id)){changed|=note.velocity!=value;note.velocity=value;}return changed;});}
bool MidiEditModel::setChannel(int value){return edit([&]{auto* current=clip();if(!current||value<1||value>16)return false;bool changed=false;for(auto& note:current->notes)if(selected.contains(note.id)){changed|=note.channel!=value;note.channel=value;}return changed;});}
bool MidiEditModel::setClipTiming(int64_t start,int64_t length,int64_t offset){return edit([&]{auto* current=clip();if(!current||start<0||length<=0||offset<0)return false;if(start==current->start&&length==current->length&&offset==current->offset)return false;
    current->start=start;current->length=length;current->offset=offset;return true;});}
bool MidiEditModel::addClip(){return edit([&]{auto& clips=engine.track(trackIndex).midiClips;if(clips.size()>=128)return false;MidiClip fresh;fresh.start=cursor;fresh.length=midiPpq*16;assignIds(fresh);clips.push_back(std::move(fresh));clipIndex=int(clips.size())-1;selected.clear();return true;});}
bool MidiEditModel::duplicateClip(){return edit([&]{auto* current=clip();auto& clips=engine.track(trackIndex).midiClips;if(!current||clips.size()>=128)return false;auto duplicate=*current;duplicate.start+=duplicate.length;assignIds(duplicate);clips.push_back(std::move(duplicate));clipIndex=int(clips.size())-1;selected.clear();return true;});}
bool MidiEditModel::importFile(const juce::File& file){MidiClip imported;if(!importSmf(file,imported,lastError))return false;
    return edit([&]{auto& clips=engine.track(trackIndex).midiClips;if(clips.size()>=128)return false;assignIds(imported);imported.start=cursor;clips.push_back(std::move(imported));clipIndex=int(clips.size())-1;selected.clear();return true;});}
bool MidiEditModel::undo(){if(!valid()||past.empty())return false;AudioEngine::MutationGuard guard(engine);future.push_back(capture());restore(past.back());past.pop_back();limitHistory();return true;}
bool MidiEditModel::redo(){if(!valid()||future.empty())return false;AudioEngine::MutationGuard guard(engine);past.push_back(capture());restore(future.back());future.pop_back();limitHistory();return true;}
void MidiEditModel::checkpointExternal(){if(!valid())return;past.push_back(capture());future.clear();limitHistory();}
}
