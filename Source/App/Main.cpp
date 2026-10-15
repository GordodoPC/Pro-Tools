#include "UI/MainComponent.h"
namespace nova {
class Application final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "NOVA DAW"; }
    const juce::String getApplicationVersion() override { return "0.7.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String& args) override {
        auto component = std::make_unique<MainComponent>();
        auto tokens = juce::StringArray::fromTokens(args, true); tokens.removeEmptyStrings();
        for (int i = 0; i + 1 < tokens.size(); ++i) {
            if (tokens[i] == "--session") component->openSession(juce::File(tokens[++i].unquoted()));
            else if(tokens[i]=="--validate-mastering-ui") component->enableMasteringUiValidation(juce::File(tokens[++i].unquoted()));
            else if(tokens[i]=="--validate-advanced-ui") component->enableAdvancedUiValidation(juce::File(tokens[++i].unquoted()));
            else if(tokens[i]=="--validate-recording-ui") component->enableRecordingUiValidation(juce::File(tokens[++i].unquoted()));
            else if(tokens[i]=="--validate-midi-ui") component->enableMidiUiValidation(juce::File(tokens[++i].unquoted()));
            else if (tokens[i] == "--validate-ui") component->enableUiValidation(juce::File(tokens[++i].unquoted()));
        }
        window = std::make_unique<Window>(component.release());
    }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override {
        if (window) if (auto* content = dynamic_cast<MainComponent*>(window->getContentComponent())) { content->requestClose(); return; }
        quit();
    }
    void anotherInstanceStarted(const juce::String&) override { if(window) window->toFront(true); }
private:
    class Window final : public juce::DocumentWindow {
    public:
        explicit Window(MainComponent* component)
            : DocumentWindow("NOVA DAW", juce::Colour(0xff12161e), allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(component,true); setResizable(true,true);
            setResizeLimits(1000,650,3840,2160); centreWithSize(1280,800); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(nova::Application)
