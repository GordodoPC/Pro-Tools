#pragma once
#include "Audio/AudioEngine.h"
namespace nova {
class Session {
public:
    static bool save(const juce::File&, const AudioEngine&, juce::String& error);
    // Call while the hardware callback is detached. All assets are validated before replacing state.
    static bool load(const juce::File&, AudioEngine&, juce::String& error);
};
}
