#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include "MixProcessor.h"

namespace nova {
struct Peak { float low = 0, high = 0; };
struct Clip {
    juce::File source;
    juce::String name;
    juce::AudioBuffer<float> samples;
    double sampleRate = 44100;
    std::vector<Peak> waveform;
    double duration() const { return samples.getNumSamples() / sampleRate; }
};
struct Track {
    bool auxiliary = false;
    bool monoBus = false;
    std::shared_ptr<Clip> clip;
    struct Region {
        uint64_t id = 0;
        std::shared_ptr<Clip> asset;
        double start = 0, offset = 0, length = 0;
        double end() const { return start + length; }
    };
    std::vector<Region> regions;
    juce::String name;
    uint32_t colour = 0xff55d8c2;
    int height = 140;
    std::atomic<float> gain{1}, pan{0}, peak{0};
    std::atomic<bool> mute{false}, solo{false};
    std::atomic<int> outputMode{0}; // 0 master stereo, 1 master left, 2 master right
    std::atomic<float> inputGain{1}, rms{0};
    std::atomic<bool> clipped{false};
    int outputTarget = -1; // -1 Master, otherwise an auxiliary channel
    struct Send { int target = -1; bool preFader = false; std::atomic<float> gain{0}; };
    std::array<Send, 2> sends;
    std::array<std::unique_ptr<MixProcessor>, 4> inserts;
    AutomationMode automationMode = AutomationMode::off;
    std::vector<AutomationLane> automation;
};

class AudioEngine final : public juce::AudioIODeviceCallback {
public:
    AudioEngine();
    // Mutations have one control-thread owner. Render uses a single CAS and never waits.
    // The guard waits for an existing render to finish on the control thread only.
    class MutationGuard {
    public:
        explicit MutationGuard(AudioEngine&);
        ~MutationGuard();
        MutationGuard(const MutationGuard&) = delete;
        MutationGuard& operator=(const MutationGuard&) = delete;
    private: AudioEngine& engine;
    };
    static constexpr int maxTracks = 64;
    static constexpr int64_t maxSessionBytes = 512LL * 1024 * 1024;
    static constexpr int maxRegionsPerTrack = 128;
    static std::unique_ptr<Clip> loadWav(const juce::File&, juce::String& error);
    // Structural operations must run with the hardware callback detached.
    bool addClip(std::unique_ptr<Clip>, juce::String& error);
    bool addSharedClip(std::shared_ptr<Clip>, juce::String& error);
    void recalculateMediaBytes();
    void clear();
    bool addAux(const juce::String&, bool mono, juce::String&);
    bool setRoute(int source, int destination, juce::String&);
    bool setSend(int source, int slot, int destination, bool preFader, juce::String&);
    bool rebuildGraph(juce::String&);
    void prepareProcessors();
    void closeEditors();
    void replaceSession(AudioEngine& validated);
    double sampleRate() const { return outputRate.load(); }
    bool needsMaintenance() const;
    bool pdcValid() const { return pdcReady.load(); }
    int latencySamples() const { return visibleLatency.load(); }
    juce::String processingError() const { return configurationError; } // control thread only
    uint64_t contentionDropouts() const;
    void setSmoothingTime(double seconds);
    int trackCount() const { return count; }
    Track& track(int index) { return tracks.at(static_cast<size_t>(index)); }
    const Track& track(int index) const { return tracks.at(static_cast<size_t>(index)); }
    double duration() const;
    uint64_t newRegionId() { return nextRegionId++; }
    void setLoop(double start, double end, bool enabled);
    std::atomic<bool> loopEnabled{false};
    std::atomic<double> loopStart{0}, loopEnd{0};
    double bpm = 120;
    int beatsPerBar = 4, beatUnit = 4;
    void play() { playing.store(true); }
    void pause() { playing.store(false); }
    void stop() { pause(); seek(0); }
    void seek(double seconds); // control thread: plugin reset is quiesced, never called by render
    bool isPlaying() const { return playing.load(); }
    double position() const { return visiblePosition.load(); }
    void setOutputRate(double rate);
    void render(float* const* output, int channels, int frames) noexcept;
    std::atomic<float> masterGain{1}, masterPeak{0};
    std::atomic<float> masterRms{0};
    std::atomic<bool> masterClipped{false}, masterMute{false};
    std::array<std::unique_ptr<MixProcessor>, 4> masterInserts;
    void audioDeviceIOCallbackWithContext(const float* const*, int,
        float* const*, int, int, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
private:
    std::array<Track, maxTracks> tracks;
    int count = 0;
    int64_t bytes = 0;
    std::atomic<bool> playing{false};
    std::atomic<double> visiblePosition{0}, requestedPosition{0};
    std::atomic<uint64_t> seekGeneration{0};
    uint64_t consumedSeek = 0;
    double renderPosition = 0, seekOrigin = 0;
    int64_t elapsedSinceSeek = 0;
    std::atomic<double> outputRate{44100}, requestedDeviceRate{44100};
    uint64_t nextRegionId = 1;
    std::array<juce::AudioBuffer<float>, maxTracks> channelBuffers;
    juce::AudioBuffer<float> masterBuffer;
    std::array<int, maxTracks> processingOrder{};
    std::array<juce::SmoothedValue<float>, maxTracks> smoothGain, smoothPan, smoothInput;
    std::array<juce::SmoothedValue<float>, maxTracks> smoothAudible;
    std::array<std::array<juce::SmoothedValue<float>, 2>, maxTracks> smoothSend;
    juce::SmoothedValue<float> smoothMaster;
    juce::SmoothedValue<float> smoothMasterAudible;
    double smoothingTime = 0.005;
    bool smoothingInitialized = false;
    std::array<float, maxTracks> blockPeaks{}, blockRms{};
    float blockMasterPeak = 0, blockMasterRms = 0;
    std::atomic<unsigned> processingGate{0};
    int mutationDepth = 0;
    DelayCompensation compensation;
    std::atomic<bool> pdcReady{true};
    std::atomic<int> visibleLatency{0};
    juce::String configurationError;
    void renderBlock(float* const*, int channels, int offset, int frames) noexcept;
};

class AudioRuntime {
public:
    AudioEngine engine;
    juce::AudioDeviceManager devices;
    juce::String initialError;
    AudioRuntime();
    ~AudioRuntime();
    void serviceMaintenance(); // message thread: rate/latency changes; never from callback
    template<class F> void editStructure(F&& edit) {
        const bool resume = engine.isPlaying();
        engine.pause();
        AudioEngine::MutationGuard mutation(engine);
        devices.removeAudioCallback(&engine);
        try { edit(); engine.prepareProcessors(); } catch (...) { devices.addAudioCallback(&engine); throw; }
        devices.addAudioCallback(&engine);
        if (resume && engine.duration() > 0) engine.play();
    }
    juce::String deviceDescription() const;
};
}
