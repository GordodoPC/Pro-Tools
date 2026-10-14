#pragma once
#include "EditorComponents.h"
namespace nova {
std::unique_ptr<juce::DocumentWindow> createAdvancedAudioEditor(AudioRuntime&,EditModel&,ApplyEdit,const juce::File& project,std::function<void()> changed);
int advanceAdvancedAudioEditorValidation(juce::DocumentWindow&);
bool validateAdvancedAudioEditorBindings(juce::DocumentWindow&);
}
