#pragma once
#include "Audio/AudioEngine.h"
#include "Audio/PluginCatalog.h"
namespace nova {
std::unique_ptr<juce::DocumentWindow> createMidiEditor(AudioRuntime&,int track,PluginCatalog&,std::function<void()> changed,const juce::File& recoveryDirectory,uint64_t selectedClip=0);
bool validateMidiEditorBindings(juce::DocumentWindow&);
}
