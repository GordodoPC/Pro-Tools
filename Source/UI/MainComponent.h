#pragma once
#include "Audio/AudioEngine.h"
#include "EditorComponents.h"
#include "MidiComponents.h"
#include "RecordingComponents.h"
#include "AudioEditComponents.h"
#include "Session/Preferences.h"
#include "Session/Session.h"
#include <functional>

namespace nova {
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
    void enableAdvancedUiValidation(const juce::File&);
    void enableRecordingUiValidation(const juce::File& outputDirectory);
    void enableMidiUiValidation(const juce::File& outputDirectory);
private:
    AudioRuntime audio;
    EditModel editor;
    TrackPanel trackPanel;
    Timeline timeline;
    Ruler ruler;
    Mixer mixer;
    ClipPanel clipPanel;
    LinkedViewport editView,trackView;
    juce::Viewport mixView;
    Divider trackDivider,clipDivider;
    Preferences preferences;
    juce::File preferencesFile;
    juce::MenuBarComponent menu;
    juce::TextButton editButton{"EDIT"}, mixButton{"MIX"}, importButton{"Importar WAV"};
    juce::TextButton playButton{"Play"}, pauseButton{"Pause"}, stopButton{"Stop"}, audioButton{"Audio"};
    juce::TextButton startButton{"|<"},loopButton{"Loop"},snapButton{"Snap"};
    juce::TextButton undoButton{"Undo"},redoButton{"Redo"};
    juce::ComboBox toolBox,gridBox,signature;
    juce::Slider verticalZoom,tempo;
    juce::Label selectionLabel,trackHeader,toolLabel,bpmLabel;
    juce::Label clock, status, masterLabel;
    juce::Slider master, zoom;
    juce::LookAndFeel_V4 look;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::DocumentWindow> midiWindow;
    std::unique_ptr<juce::DocumentWindow> recordingWindow;
    std::unique_ptr<juce::DocumentWindow> advancedWindow;
    void openAdvancedEditor();
    void advancedUiValidation();
    bool validationAdvanced=false,validationAdvancedBindings=false;
    void addAudioTrack();
    void openRecordingEditor();
    void closeRecording();
    void recordingUiValidation();
    bool validationRecording=false;
    void addMidiTrack();
    void openMidiEditor();
    void closeMidiEditing();
    void midiUiValidation();
    bool validationMidi=false,validationMidiBindings=false,validationMidiAudio=false;
    juce::File currentSession;
    bool mixVisible = false, dirty = false;
    uint64_t lastAutomationRevision=0;
    void timerCallback() override;
    void switchView(bool);
    void chooseImport();
    void chooseOpen();
    void saveSession(bool saveAs);
    void refreshTracks();
    void refreshEdited();
    bool applyEdit(std::function<bool()>);
    void editCommand(int);
    void setLoopFromSelection();
    bool validateEditor();
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
    bool validationPhase3 = false;
    bool validationEditing=false,validationLoop=false;
    double validationPausePosition = 0;
};
}
