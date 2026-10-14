#pragma once
#include "Audio/AudioEngine.h"
#include <set>
#include <functional>
namespace nova {
class MidiEditModel {
public:
    MidiEditModel(AudioEngine&,int track);
    int clipIndex=0;
    bool snap=true;
    int64_t grid=midiPpq/4,cursor=0;
    std::set<uint64_t> selected;
    MidiClip* clip();
    const MidiClip* clip() const;
    bool valid() const;
    void select(uint64_t id,bool extend=false);
    void selectAll();
    int64_t quantize(int64_t) const;
    bool addNote(int64_t tick,int pitch,int64_t length,int velocity,int channel);
    bool moveSelected(int64_t delta,int semitones);
    bool resizeSelected(int64_t delta);
    bool eraseSelected();
    bool copy();
    bool paste(int64_t tick);
    bool quantizeSelected();
    bool setVelocity(int);
    bool setChannel(int);
    bool setClipTiming(int64_t start,int64_t length,int64_t offset);
    bool addClip();
    bool duplicateClip();
    bool importFile(const juce::File&);
    bool undo();bool redo();
    bool canUndo() const { return !past.empty(); }
    bool canRedo() const { return !future.empty(); }
    bool canPaste() const { return !clipboard.empty(); }
    juce::String error() const { return lastError; }
    void checkpointExternal();
private:
    struct State {std::vector<MidiClip> clips;int clipIndex=0;std::set<uint64_t> selected;};
    AudioEngine& engine;int trackIndex;uint64_t generation;
    std::vector<State> past,future;
    std::vector<MidiNote> clipboard;
    juce::String lastError;
    State capture() const;
    void restore(const State&);
    bool edit(const std::function<bool()>&);
    void limitHistory();
    void assignIds(MidiClip&);
};
}
