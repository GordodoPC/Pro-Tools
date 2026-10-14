#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <iostream>

int main()
{
    juce::AudioBuffer<float> buffer{2, 64};
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.applyGain(0.5f);
    if (std::abs(buffer.getSample(0, 0) - 0.5f) > 0.00001f
        || buffer.getSample(1, 0) != 0.0f)
        return 1;
    std::cout << "JUCE " << juce::SystemStats::getJUCEVersion() << ": PASS\n";
}
