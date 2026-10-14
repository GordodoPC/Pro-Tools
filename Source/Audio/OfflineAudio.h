#pragma once
#include "AudioEngine.h"
namespace nova {
class OfflineAudio {
public:
    // Pure/offline jobs. Caller owns immutable sources and polls cancellation/progress.
    static std::shared_ptr<Clip> transform(const Track::Region&,double stretch,double semitones,double cents,
        const juce::File& cache,juce::String&,const std::atomic<bool>* cancel=nullptr,std::atomic<float>* progress=nullptr);
    static std::shared_ptr<Clip> consolidate(const std::vector<Track::Region>&,double start,double end,double rate,
        const juce::File& destination,juce::String&,const std::atomic<bool>* cancel=nullptr,std::atomic<float>* progress=nullptr);
    // Snapshot/instantiation happens on control/message thread. Render occurs on worker.
    static std::unique_ptr<AudioEngine> snapshot(const AudioEngine&,const juce::File& temporaryDirectory,juce::String&);
    static bool bounce(AudioEngine& offline,double start,double end,const juce::File&,juce::String&,
        const std::atomic<bool>* cancel=nullptr,std::atomic<float>* progress=nullptr);
};
}
