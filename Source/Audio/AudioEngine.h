#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

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
    std::unique_ptr<Clip> clip;
    std::atomic<float> gain{1}, pan{0}, peak{0};
    std::atomic<bool> mute{false}, solo{false};
};

class AudioEngine final : public juce::AudioIODeviceCallback {
public:
    static constexpr int maxTracks = 64;
    static constexpr int64_t maxSessionBytes = 512LL * 1024 * 1024;
    static std::unique_ptr<Clip> loadWav(const juce::File&, juce::String& error);
    // Structural operations must run with the hardware callback detached.
    bool addClip(std::unique_ptr<Clip>, juce::String& error);
    void clear();
    int trackCount() const { return count; }
    Track& track(int index) { return tracks.at(static_cast<size_t>(index)); }
    const Track& track(int index) const { return tracks.at(static_cast<size_t>(index)); }
    double duration() const;
    void play() { playing.store(true); }
    void pause() { playing.store(false); }
    void stop() { pause(); seek(0); }
    void seek(double seconds);
    bool isPlaying() const { return playing.load(); }
    double position() const { return visiblePosition.load(); }
    void setOutputRate(double rate) { outputRate = rate > 0 ? rate : 44100; }
    void render(float* const* output, int channels, int frames) noexcept;
    std::atomic<float> masterGain{1}, masterPeak{0};
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
    double renderPosition = 0, outputRate = 44100;
};

class AudioRuntime {
public:
    AudioEngine engine;
    juce::AudioDeviceManager devices;
    juce::String initialError;
    AudioRuntime();
    ~AudioRuntime();
    template<class F> void editStructure(F&& edit) {
        engine.pause();
        devices.removeAudioCallback(&engine);
        edit();
        devices.addAudioCallback(&engine);
    }
    juce::String deviceDescription() const;
};
}
