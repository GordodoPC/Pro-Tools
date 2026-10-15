#include "UI/MainComponent.h"
#include "AppPaths.h"
#include "Diagnostics.h"
namespace nova {
class Application final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return AppPaths::releaseName; }
    const juce::String getApplicationVersion() override { return "0.7.0-alpha"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String& args) override {
        auto early=juce::StringArray::fromTokens(args,true);early.removeEmptyStrings();for(int i=0;i+1<early.size();++i)if(early[i]=="--data-dir"){const auto path=early[++i].unquoted();if(!juce::File::isAbsolutePath(path)){setApplicationReturnValue(2);quit();return;}_wputenv_s(L"NOVA_DAW_DATA_DIR",path.toWideCharPointer());}
        if(!AppPaths::data().createDirectory().wasOk()||!AppPaths::data().hasWriteAccess()){setApplicationReturnValue(3);juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"NOVA DAW Alpha","Pasta de dados sem permissao de escrita.");quit();return;}
        diagnostics=std::make_unique<Diagnostics>();
        if(early.contains("--test-crash-report")&&early.contains("--data-dir")){Diagnostics::testCrash();return;}
        if(early.contains("--diagnostics-smoke")){const auto report=Diagnostics::report("self-test","Teste local de logs e relatorio; sem envio automatico.");diagnostics->flush();setApplicationReturnValue(report.existsAsFile()?0:4);quit();return;}
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
    void shutdown() override { window.reset();diagnostics.reset(); }
    void unhandledException(const std::exception* error,const juce::String& source,int line)override{Diagnostics::report("exception",(error?juce::String(error->what()):"Unknown exception")+" | "+source+":"+juce::String(line));setApplicationReturnValue(5);quit();}
    void systemRequestedQuit() override {
        if (window) if (auto* content = dynamic_cast<MainComponent*>(window->getContentComponent())) { content->requestClose(); return; }
        quit();
    }
    void anotherInstanceStarted(const juce::String&) override { if(window) window->toFront(true); }
private:
    class Window final : public juce::DocumentWindow {
    public:
        explicit Window(MainComponent* component)
            : DocumentWindow(AppPaths::releaseName, juce::Colour(0xff12161e), allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(component,true); setResizable(true,true);
            setResizeLimits(1000,650,3840,2160); centreWithSize(1280,800); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
    std::unique_ptr<Diagnostics> diagnostics;
};
}
START_JUCE_APPLICATION(nova::Application)
