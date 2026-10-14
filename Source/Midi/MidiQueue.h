#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <type_traits>
namespace nova {
// Fixed multiple-producer / single-consumer mailbox. Each slot publishes its
// sequence after its payload. A stalled producer can delay delivery; the audio
// consumer never waits. Writers make at most eight reservation attempts.
// Construct/destroy outside audio, stop producers before destruction; no reset
// against live producers. Session generation tags invalidate old messages.
template<class Event,size_t Capacity=2048> class MidiQueue {
    static_assert(Capacity>0&&(Capacity&(Capacity-1))==0);
    static_assert(std::is_trivially_copyable_v<Event>);
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    struct Cell {std::atomic<uint64_t> sequence{0};Event event{};};
    std::array<Cell,Capacity> cells;
    alignas(64) std::atomic<uint64_t> reserved{0};
    alignas(64) uint64_t consumed=0;
public:
    MidiQueue(){for(uint64_t i=0;i<Capacity;++i)cells[i].sequence.store(i,std::memory_order_relaxed);}
    bool tryPush(const Event& event)noexcept{
        auto position=reserved.load(std::memory_order_relaxed);
        for(int attempt=0;attempt<8;++attempt){auto& cell=cells[position&(Capacity-1)];const auto sequence=cell.sequence.load(std::memory_order_acquire);
            if(sequence==position){if(reserved.compare_exchange_weak(position,position+1,std::memory_order_relaxed)){
                cell.event=event;cell.sequence.store(position+1,std::memory_order_release);return true;
            }}else if(sequence<position)return false;else position=reserved.load(std::memory_order_relaxed);
        }
        return false;
    }
    bool tryPop(Event& event)noexcept{
        auto& cell=cells[consumed&(Capacity-1)];if(cell.sequence.load(std::memory_order_acquire)!=consumed+1)return false;
        event=cell.event;cell.sequence.store(consumed+Capacity,std::memory_order_release);++consumed;return true;
    }
};
struct LiveMidiEvent {
    uint64_t generation=0;
    uint64_t device=0;
    double targetTime=-1; // monotonic seconds, or -1 for explicitly injected offsets
    int track=-1,sampleOffset=0;
    std::array<uint8_t,3> bytes{};
    uint8_t size=3;
};
struct CapturedMidiEvent {
    uint64_t generation=0;
    int track=-1;
    int64_t tick=0;
    std::array<uint8_t,3> bytes{};
    uint8_t size=3;
};
}
