#include "EditModel.h"
#include <algorithm>
#include <cmath>
namespace nova {
double EditModel::quantize(double time) const {
    if (!std::isfinite(time)) return 0;
    const double grid = 60.0 / engine.bpm * (4.0 / engine.beatUnit) * gridBeats;
    return std::max(0.0, snap && grid > 0 ? std::round(time / grid) * grid : time);
}
Track::Region* EditModel::selected() {
    if (selectedTrack < 0 || selectedTrack >= engine.trackCount()) return nullptr;
    for (auto& region : engine.track(selectedTrack).regions) if (region.id == selectedRegion) return &region;
    return nullptr;
}
void EditModel::select(int track, uint64_t id) { selectedTrack = track; selectedRegion = id; rangeStart=rangeEnd=0; }
EditModel::State EditModel::capture() const {
    State state{{}, selectedTrack, selectedRegion};
    for (int i = 0; i < engine.trackCount(); ++i) {
        const auto& t = engine.track(i); state.rows.push_back({t.regions,t.name,t.colour,t.height});
    }
    return state;
}
void EditModel::restore(const State& state) {
    for (int i = 0; i < engine.trackCount() && i < static_cast<int>(state.rows.size()); ++i) {
        const auto& row = state.rows[static_cast<size_t>(i)]; auto& t = engine.track(i);
        t.regions=row.regions; t.name=row.name; t.colour=row.colour; t.height=row.height;
    }
    selectedTrack=state.selectedTrack; selectedRegion=state.selectedRegion;
    if (engine.loopEnd.load()>engine.duration()) engine.loopEnabled=false;
}
void EditModel::checkpoint() { past.push_back(capture()); if(past.size()>64) past.erase(past.begin()); future.clear(); }
void EditModel::reset() { past.clear(); future.clear(); clipboard.reset(); select(-1,0); rangeStart=rangeEnd=0; }
bool EditModel::move(double start, int destination) {
    auto* r=selected();
    if (!r || !std::isfinite(start) || destination<0 || destination>=engine.trackCount() || engine.track(destination).auxiliary || engine.track(destination).midiTrack) return false;
    if (destination!=selectedTrack && engine.track(destination).regions.size()>=AudioEngine::maxRegionsPerTrack) return false;
    start=std::max(0.0,start);
    if (start+r->length>86400) return false;
    if(destination==selectedTrack && std::abs(r->start-start)<1e-9) return false;
    checkpoint(); auto copy=*r; copy.start=start;
    auto& source=engine.track(selectedTrack).regions;
    source.erase(std::remove_if(source.begin(),source.end(),[&](const auto& item){return item.id==copy.id;}),source.end());
    engine.track(destination).regions.push_back(std::move(copy)); selectedTrack=destination;
    return true;
}
bool EditModel::trim(double start, double end) {
    auto* r=selected(); if(!r || !std::isfinite(start) || !std::isfinite(end)) return false;
    const double earliest=std::max(0.0,r->start-r->offset);
    const double latest=std::min(86400.0,r->start+(r->asset->duration()-r->offset));
    start=std::clamp(start,earliest,latest); end=std::clamp(end,start,latest);
    if(end-start<1/r->asset->sampleRate || (start==r->start && end==r->end())) return false;
    checkpoint(); r->offset+=start-r->start; r->start=start; r->length=end-start; return true;
}
bool EditModel::split(double time) {
    auto* r=selected(); if(!r || !std::isfinite(time) || time<=r->start || time>=r->end()
        || engine.track(selectedTrack).regions.size()>=AudioEngine::maxRegionsPerTrack) return false;
    const double left=time-r->start;
    if(left<1/r->asset->sampleRate || r->length-left<1/r->asset->sampleRate) return false;
    checkpoint(); auto right=*r; right.id=engine.newRegionId(); right.start=time;
    right.offset+=left; right.length-=left; r->length=left;
    engine.track(selectedTrack).regions.push_back(std::move(right)); return true;
}
bool EditModel::erase() {
    if(!selected()) return false; checkpoint(); auto& rows=engine.track(selectedTrack).regions;
    rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& item){return item.id==selectedRegion;}),rows.end());
    selectedRegion=0; return true;
}
bool EditModel::copy() { if(auto* r=selected()){clipboard=*r;return true;} return false; }
bool EditModel::paste(int track, double time) {
    if(!clipboard || track<0 || track>=engine.trackCount() || engine.track(track).auxiliary || engine.track(track).midiTrack || !std::isfinite(time)
        || engine.track(track).regions.size()>=AudioEngine::maxRegionsPerTrack || std::max(0.0,time)+clipboard->length>86400) return false;
    checkpoint(); auto r=*clipboard; r.id=engine.newRegionId(); r.start=std::max(0.0,time);
    selectedTrack=track; selectedRegion=r.id; engine.track(track).regions.push_back(std::move(r)); return true;
}
bool EditModel::undo() { if(past.empty()) return false; future.push_back(capture()); restore(past.back()); past.pop_back(); return true; }
bool EditModel::redo() { if(future.empty()) return false; past.push_back(capture()); restore(future.back()); future.pop_back(); return true; }
void EditModel::rename(int i,const juce::String& name) { if(i<0 || i>=engine.trackCount() || name.trim().isEmpty() || engine.track(i).name==name.trim().substring(0,80))return;checkpoint();engine.track(i).name=name.trim().substring(0,80); }
void EditModel::recolour(int i,uint32_t colour) { if(i<0 || i>=engine.trackCount() || engine.track(i).colour==colour)return;checkpoint();engine.track(i).colour=colour; }
void EditModel::resizeTrack(int i,int height) { if(i<0 || i>=engine.trackCount() || engine.track(i).height==std::clamp(height,100,320))return;checkpoint();engine.track(i).height=std::clamp(height,100,320); }
}
