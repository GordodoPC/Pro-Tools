#pragma once
#include "Audio/AudioEngine.h"
#include <optional>
namespace nova {
// All mutations and undo/redo must execute with the hardware callback detached.
class EditModel {
public:
    explicit EditModel(AudioEngine& e) : engine(e) {}
    int selectedTrack = -1;
    uint64_t selectedRegion = 0;
    double rangeStart = 0, rangeEnd = 0;
    bool snap = true;
    double gridBeats = 1;
    double quantize(double seconds) const;
    Track::Region* selected();
    void select(int track, uint64_t id);
    bool move(double start, int destinationTrack);
    bool trim(double newStart, double newEnd);
    bool split(double time);
    bool erase();
    bool copy();
    bool paste(int track, double time);
    bool undo();
    bool redo();
    bool canUndo() const { return !past.empty(); }
    bool canRedo() const { return !future.empty(); }
    bool canPaste() const { return clipboard.has_value(); }
    void reset();
    void rename(int track, const juce::String&);
    void recolour(int track, uint32_t);
    void resizeTrack(int track, int height);
private:
    struct Row { std::vector<Track::Region> regions; juce::String name; uint32_t colour; int height; };
    struct State { std::vector<Row> rows; int selectedTrack; uint64_t selectedRegion; };
    AudioEngine& engine;
    std::vector<State> past, future;
    std::optional<Track::Region> clipboard;
    State capture() const;
    void restore(const State&);
    void checkpoint();
};
}
