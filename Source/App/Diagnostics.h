#pragma once
#include <juce_events/juce_events.h>
#include <atomic>
#include <array>
namespace nova {
// Producers only copy into a bounded MPSC queue. Disk/JSON/rotation run on control.
class Diagnostics final : public juce::Logger,private juce::Timer {
public:
    Diagnostics();
    ~Diagnostics()override;
    void logMessage(const juce::String&)override;
    void flush();
    static juce::File report(const juce::String& category,const juce::String& details);
    static juce::File directory();
    static void testCrash();
    uint64_t dropped()const{return overflow.load();}
private:
    struct Record {uint64_t tick=0;uint32_t thread=0;std::array<char,768> text{};};
    struct Slot {std::atomic<size_t> sequence{0};Record record;};
    std::array<Slot,256> queue;
    std::atomic<size_t> write{0};size_t read=0;
    std::atomic<uint64_t> overflow{0};uint64_t lastOverflow=0;
    juce::File logFile;bool previousUnclean=false;
    void timerCallback()override{flush();}
    void rotate();
    void writeMarker(bool clean);
};
}
