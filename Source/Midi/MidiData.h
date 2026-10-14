#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
namespace nova {
constexpr int midiPpq=960;
constexpr int maximumMidiEventsPerBlock=256;
struct MidiNote {
    uint64_t id=0;
    int64_t tick=0,length=midiPpq/4;
    int pitch=60,velocity=100,channel=1;
};
struct MidiEvent {
    int64_t tick=0;
    std::array<uint8_t,3> bytes{};
    uint8_t size=3;
};
struct MidiClip {
    uint64_t id=0;
    juce::String name="MIDI";
    int64_t start=0,offset=0,length=midiPpq*4;
    std::vector<MidiNote> notes;
    std::vector<MidiEvent> controls;
};
double ticksToSeconds(int64_t ticks,double bpm) noexcept;
int64_t secondsToTicks(double seconds,double bpm) noexcept;
int64_t ticksToSamples(int64_t ticks,double bpm,double rate) noexcept;
bool compileMidi(const std::vector<MidiClip>&,std::vector<MidiEvent>&,juce::String&);
// Destination capacity must be prepared with ensureSize(8192) off the callback.
// Overflow produces all-notes-off instead of allowing an allocation/stuck note.
bool scheduleMidi(const std::vector<MidiEvent>&,double seconds,double bpm,double rate,
                  int frames,juce::MidiBuffer&) noexcept;
void midiPanic(juce::MidiBuffer&,int sample=0) noexcept;
juce::var midiClipState(const MidiClip&);
bool readMidiClip(const juce::var&,MidiClip&,juce::String&);
bool importSmf(const juce::File&,MidiClip&,juce::String&);
bool exportSmf(const juce::File&,const std::vector<MidiClip>&,double bpm,int beats,int unit,juce::String&);
}
