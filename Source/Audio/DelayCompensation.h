#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <juce_core/juce_core.h>

namespace nova {
// Configuration allocates on the control thread. Audio operations are bounded,
// single-thread owned and allocation-free. Reset invalidates cells in O(1).
class StereoDelay {
public:
    static constexpr int maximumDelay = 262144;
    void prepare(int samples);
    void reset() noexcept;
    std::array<float,2> process(float left, float right) noexcept;
    int latency() const noexcept { return static_cast<int>(cells.size()); }
    size_t bytes() const noexcept { return cells.size()*sizeof(Cell); }
private:
    struct Cell { std::array<float,2> value{}; uint64_t generation = 0; };
    std::vector<Cell> cells;
    size_t write = 0;
    uint64_t generation = 1;
};

struct PdcNode {
    bool auxiliary = false;
    int output = -1, latency = 0;
    std::array<int,2> sends{-1,-1};
    int sidechainSource=-1;
    std::array<int,4> insertLatencies{};
};
class DelayCompensation {
public:
    static constexpr int maximumNodes = 64;
    static constexpr size_t maximumMemory = 128*1024*1024;
    // Build a fresh candidate, then move it into the engine while processing is suspended.
    bool build(const std::vector<PdcNode>&, int masterInsertLatency, juce::String& error);
    void reset() noexcept;
    std::array<int, maximumNodes> order{}, inputLatency{}, outputLatency{};
    std::array<std::array<int,3>,maximumNodes> targets{};
    std::array<std::array<StereoDelay,3>, maximumNodes> edges;
    struct SidechainDelays {std::array<StereoDelay,maximumNodes> sourceAlignment,detectorAlignment;std::array<std::array<StereoDelay,4>,maximumNodes> detectorInserts;};
    std::unique_ptr<SidechainDelays> sidechain;
    int masterInputLatency = 0, totalLatency = 0;
    size_t allocatedBytes = 0;
};
}
