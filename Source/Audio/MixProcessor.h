#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

namespace nova {
constexpr int mixBlockSize = 256;
enum class EffectKind { equalizer, compressor, gate, delay, reverb, vst3 };
struct ParameterSpec { const char* name; float minimum, maximum, initial; };

// Owned by the channel. Prepare, editor and state operations require a detached callback.
class MixProcessor {
public:
    explicit MixProcessor(EffectKind);
    ~MixProcessor();
    EffectKind kind;
    std::atomic<bool> bypass{false};
    std::atomic<bool> faulted{false};
    std::atomic<float> reduction{0};
    std::array<std::atomic<float>, 24> parameters{};
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;
    juce::MemoryBlock savedState;
    juce::String failure;
    int parameterCount() const;
    ParameterSpec spec(int) const;
    float parameter(int) const;
    void setParameter(int, float);
    juce::String name() const;
    void prepare(double);
    void reset(bool resetPlugin = true);
    void process(juce::AudioBuffer<float>&, int frames) noexcept;
    double magnitude(double frequency, double rate) const;
    juce::var state() const;
    static std::unique_ptr<MixProcessor> restore(const juce::var&, double, juce::String&);
    void openEditor();
    void closeEditor();
private:
    double sampleRate = 44100;
    std::array<juce::SmoothedValue<float>, 24> smoothed;
    juce::SmoothedValue<float> wet;
    std::array<std::array<float, 6>, 7> coefficients{};
    std::array<std::array<std::array<float, 2>, 7>, 2> filterState{};
    juce::AudioBuffer<float> delayBuffer;
    std::vector<uint32_t> delayGeneration;
    uint32_t currentDelayGeneration = 1;
    int delayWrite = 0, holdSamples = 0;
    float envelope = 0, gateGain = 0;
    juce::Reverb reverberator;
    juce::MidiBuffer midi;
    std::array<std::array<float, mixBlockSize>, 2> dry{};
    std::unique_ptr<juce::DocumentWindow> editorWindow;
    void updateEq(const std::array<float, 24>&, double);
};

enum class AutomationMode { off, read, write, touch, latch };
struct AutomationPoint { double time = 0; float value = 0; };
struct AutomationLane {
    // -1 volume, -2 pan, >=0 insert index; parameter is the normalized VST index/native value.
    int target = -1, parameter = 0;
    std::vector<AutomationPoint> points;
    float evaluate(double time, float fallback) const noexcept;
};
}
