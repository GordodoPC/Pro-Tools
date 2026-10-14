#pragma once
#include "Audio/AudioEngine.h"
namespace nova {
std::unique_ptr<juce::DocumentWindow> createRecordingEditor(AudioRuntime&,int track,const juce::File& folder,std::function<void()> changed);
bool validateRecordingEditorBindings(juce::DocumentWindow&);
}
