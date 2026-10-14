#pragma once
#include "MidiData.h"
#include <juce_audio_devices/juce_audio_devices.h>
namespace nova {
class AudioEngine;
class MidiPort final : private juce::MidiInputCallback {
public:
    MidiPort(AudioEngine&,const juce::String& identifier);
    ~MidiPort() override;
    bool available() const noexcept { return input!=nullptr; }
    juce::String identifier;
private:
    AudioEngine& engine;uint64_t token=0;std::unique_ptr<juce::MidiInput> input;
    void handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage&) override;
};
class MidiRecorder {
public:
    explicit MidiRecorder(AudioEngine&);
    bool start(const juce::File& recoveryDirectory,juce::String&);
    bool stop(juce::String&);
    bool saveRecovery(const juce::File&,juce::String&);
    void service();
    bool isActive() const noexcept { return active; }
    juce::String status() const { return lastStatus; }
private:
    struct Held {int64_t tick=0;int velocity=0;bool active=false;};
    using HeldNotes=std::array<std::array<Held,2048>,64>;
    AudioEngine& engine;std::unique_ptr<HeldNotes> held;
    std::array<MidiClip,64> takes;
    std::array<bool,64> armed{};
    bool active=false,interrupted=false,unsaved=false,restoreLoop=false;
    uint64_t generation=0,captureDropsAtStart=0;int64_t startTick=0,lastTick=0;double tempo=120,oldLoopStart=0,oldLoopEnd=0;
    juce::File folder;juce::String lastStatus;
    void drain();
    void finishNote(int track,int channel,int pitch,int64_t end);
};
}
