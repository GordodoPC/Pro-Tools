#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
namespace nova {
class AudioEngine;
struct RecordingOptions {
    bool punch=false,loop=false;
    double punchIn=0,punchOut=4,preRoll=0,postRoll=0;
    double loopStart=0,loopEnd=4;
    int64_t failAfterFrames=-1; // deterministic writer-failure fixture; UI never enables it
};
class AudioRecorder {
public:
    explicit AudioRecorder(AudioEngine&);
    ~AudioRecorder();
    bool start(const juce::File&,int availableInputs,const RecordingOptions&,juce::String&);
    bool stop(juce::String&);
    bool recover(const juce::File& captureDirectory,juce::String&);
    void service();
    void capture(const float* const* input,int channels,int frames,double songSeconds) noexcept;
    bool isActive() const noexcept;
    juce::String status() const;
    uint64_t overflowCount() const noexcept;
    juce::File captureDirectory() const;
private:
    struct Impl;std::unique_ptr<Impl> impl;AudioEngine& engine;juce::String lastStatus;
};
}
