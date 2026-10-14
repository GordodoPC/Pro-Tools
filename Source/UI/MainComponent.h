#pragma once
#include "Audio/AudioEngine.h"
#include "Session/Session.h"
#include <functional>

namespace nova {
class TrackControls final : public juce::Component {
public:
    TrackControls(AudioEngine&, int index, bool vertical, std::function<void()> changed);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
    bool validateBindings();
private:
    AudioEngine& engine;
    int index;
    bool vertical;
    juce::Label name, gainLabel, panLabel;
    juce::Slider gain, pan;
    juce::TextButton mute{"M"}, solo{"S"};
};

class Timeline final : public juce::Component {
public:
    explicit Timeline(AudioEngine&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void rebuild(std::function<void()> changed);
    void setZoom(double value);
    bool validateBindings();
    double pixelsPerSecond = 90;
private:
    AudioEngine& engine;
    std::vector<std::unique_ptr<TrackControls>> controls;
};

class Mixer final : public juce::Component {
public:
    explicit Mixer(AudioEngine&);
    void paint(juce::Graphics&) override;
    void rebuild(std::function<void()> changed);
    bool validateBindings();
private:
    AudioEngine& engine;
    std::vector<std::unique_ptr<TrackControls>> controls;
};

class MainComponent final : public juce::Component, public juce::FileDragAndDropTarget,
                            private juce::Timer, private juce::MenuBarModel {
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    void importFiles(const juce::Array<juce::File>&);
    bool openSession(const juce::File&);
    void requestClose();
    void enableUiValidation(const juce::File& outputDirectory);
private:
    AudioRuntime audio;
    Timeline timeline;
    Mixer mixer;
    juce::Viewport editView, mixView;
    juce::MenuBarComponent menu;
    juce::TextButton editButton{"EDIT"}, mixButton{"MIX"}, importButton{"Importar WAV"};
    juce::TextButton playButton{"Play"}, pauseButton{"Pause"}, stopButton{"Stop"}, audioButton{"Audio"};
    juce::Label clock, status, masterLabel;
    juce::Slider master, zoom;
    juce::LookAndFeel_V4 look;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File currentSession;
    bool mixVisible = false, dirty = false;
    void timerCallback() override;
    void switchView(bool);
    void chooseImport();
    void chooseOpen();
    void saveSession(bool saveAs);
    void refreshTracks();
    void showError(const juce::String&);
    void confirmReplacement(std::function<void()>);
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int, const juce::String&) override;
    void menuItemSelected(int, int) override;
    juce::File validationDirectory;
    int validationStep = -1, callbackTicks = 0;
    bool validationSawAudio = false, validationSawPlay = false, validationSawPause = false;
    bool validationImported = false, validationResume = false;
    bool validationControls = false;
    double validationPausePosition = 0;
};
}
