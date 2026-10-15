#pragma once
#include "Audio/AudioEngine.h"
namespace nova {
std::unique_ptr<juce::DocumentWindow> createMasteringEditor(AudioEngine&,std::function<void()> changed);
bool validateMasteringEditor(juce::DocumentWindow&,const juce::File&);
int advanceMasteringValidation(juce::DocumentWindow&);
}
