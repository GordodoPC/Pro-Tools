#pragma once
#include "Audio/PluginCatalog.h"
#include "Session/EditModel.h"
namespace nova {
using MixEdit = std::function<bool(std::function<bool()>)>;
class ChannelStrip final : public juce::Component {
public:
    ChannelStrip(AudioEngine&,EditModel&,int,PluginCatalog&,MixEdit,std::function<void()>);
    ~ChannelStrip() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
    void closeEditors();
    bool validateBindings();
private:
    AudioEngine& engine;EditModel& edit;int index;PluginCatalog& catalog;MixEdit apply;std::function<void()> changed;
    juce::Label name,input,peakLabel;
    juce::Slider gain,pan,inputGain;
    juce::TextButton mute{"M"},solo{"S"},clip{"CLIP"},colour{"Cor"},automation{"Auto Off"};
    juce::ComboBox output;
    std::array<juce::TextButton,4> inserts;
    std::array<juce::TextButton,2> sends;
    std::unique_ptr<juce::DocumentWindow> toolWindow;
    std::array<std::unique_ptr<MixProcessor>,4>& chain();
    void insertMenu(int);
    void sendEditor(int);
    void automationEditor();
    void showError(const juce::String&);
};
}
