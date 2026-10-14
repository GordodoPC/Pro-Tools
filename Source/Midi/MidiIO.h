#pragma once
#include "MidiQueue.h"
#include "MidiData.h"
namespace nova {
inline uint64_t midiDeviceToken(const juce::String& identifier){
    if(identifier.isEmpty())return 0;
    uint64_t hash=14695981039346656037ull;const auto* text=identifier.toRawUTF8();
    for(;*text;++text){hash^=static_cast<uint8_t>(*text);hash*=1099511628211ull;}return hash?hash:1;
}
struct MidiIO {
    MidiQueue<LiveMidiEvent> incoming;
    MidiQueue<CapturedMidiEvent> captured;
    std::array<std::atomic<uint64_t>,64> routes{};
    std::array<juce::MidiBuffer,64> live;
    std::array<bool,64> overflow{};
    std::array<LiveMidiEvent,2048> pending{};
    int pendingCount=0;
    std::atomic<uint64_t> clockSequence{0};
    std::atomic<double> renderWallTime{0},clockRate{44100};
    std::atomic<int> clockFrames{0};
    std::atomic<uint64_t> generation{1},droppedInput{0},droppedCapture{0};
    std::atomic<bool> panic{false};
    MidiIO(){for(auto& buffer:live)buffer.ensureSize(8192);}
};
}
