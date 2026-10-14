#pragma once
#include "Audio/AudioEngine.h"
#include <optional>
#include <set>
namespace nova {
// All mutations and undo/redo must execute with the hardware callback detached.
class EditModel {
public:
    explicit EditModel(AudioEngine& e) : engine(e) {}
    int selectedTrack = -1;
    uint64_t selectedRegion = 0;
    std::set<uint64_t> selection;
    bool ripple=false;
    double rangeStart = 0, rangeEnd = 0;
    bool snap = true;
    double gridBeats = 1;
    double quantize(double seconds) const;
    Track::Region* selected();
    void select(int track, uint64_t id);
    void toggleSelection(int track,uint64_t id);
    void selectAll();
    void selectRange(int track,double start,double end);
    bool isSelected(uint64_t id)const{return selection.contains(id)||id==selectedRegion;}
    bool move(double start, int destinationTrack);
    bool trim(double newStart, double newEnd);
    bool split(double time);
    bool erase();
    bool copy();
    bool paste(int track, double time);
    bool duplicate();
    bool slip(double deltaSeconds);
    bool setFades(double in,double out,FadeCurve inCurve,FadeCurve outCurve);
    bool crossfade(double seconds,FadeCurve);
    bool comp(uint64_t take,double start,double end,double crossfade,juce::String& error);
    bool adoptTransform(const Track::Region& expected,std::shared_ptr<Clip>,double stretch,double semitones,double cents,juce::String&);
    bool revertOriginal();
    bool adoptConsolidated(std::shared_ptr<Clip>,double start,double end,juce::String&);
    bool undo();
    bool redo();
    bool canUndo() const { return !past.empty(); }
    bool canRedo() const { return !future.empty(); }
    bool canPaste() const { return !clipboard.empty(); }
    void reset();
    void rename(int track, const juce::String&);
    void recolour(int track, uint32_t);
    void resizeTrack(int track, int height);
private:
    struct Row { std::vector<Track::Region> regions; juce::String name; uint32_t colour; int height;uint64_t activeTake; };
    struct State { std::vector<Row> rows; int selectedTrack; uint64_t selectedRegion;std::set<uint64_t> selection; };
    AudioEngine& engine;
    std::vector<State> past, future;
    struct Copied {int track;Track::Region region;};
    std::vector<Copied> clipboard;
    std::vector<Copied> selectedCopies()const;
    void fitFades(Track::Region&);
    State capture() const;
    void restore(const State&);
    void checkpoint();
    void boundHistory();
};
}
