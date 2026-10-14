#include "MainComponent.h"
#include <algorithm>
#include <cmath>

namespace nova {
namespace {
const juce::Colour background{0xff12161e}, panel{0xff1b222e}, line{0xff30394a}, accent{0xff55d8c2};
void meter(juce::Graphics& g, juce::Rectangle<float> bounds, float level, bool horizontal) {
    g.setColour(juce::Colour(0xff090c12)); g.fillRoundedRectangle(bounds, 2);
    g.setColour(level > 1 ? juce::Colour(0xffff6d73) : accent);
    const float amount = juce::jlimit(0.0f, 1.0f, level);
    if (horizontal) g.fillRect(bounds.withWidth(bounds.getWidth() * amount));
    else g.fillRect(bounds.removeFromBottom(bounds.getHeight() * amount));
}
}
MainComponent::MainComponent()
    : editor(audio.engine),
      trackPanel(audio.engine, editor, [this](auto edit) { return applyEdit(std::move(edit)); }),
      timeline(audio.engine, editor, [this](auto edit) { return applyEdit(std::move(edit)); }),
      ruler(audio.engine, editor, timeline),
      mixer(audio.engine, editor, [this](auto edit) { return applyEdit(std::move(edit)); }),
      clipPanel(audio.engine, editor), menu(this) {
    look.setColour(juce::ResizableWindow::backgroundColourId, background);
    look.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff293444));
    look.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    look.setColour(juce::Slider::thumbColourId, accent); look.setColour(juce::Slider::trackColourId, accent);
    look.setColour(juce::Label::textColourId, juce::Colour(0xffd4deeb));
    look.setColour(juce::PopupMenu::backgroundColourId, panel);
    setLookAndFeel(&look); setWantsKeyboardFocus(true);
    for (auto* child : std::initializer_list<juce::Component*>{&menu,&editButton,&mixButton,&importButton,&playButton,&pauseButton,&stopButton,&audioButton,&clock,&status,&masterLabel,&master,&zoom,&editView,&trackView,&mixView,&ruler,&clipPanel,&trackDivider,&clipDivider,&startButton,&loopButton,&snapButton,&undoButton,&redoButton,&toolBox,&gridBox,&signature,&verticalZoom,&tempo,&selectionLabel,&trackHeader,&toolLabel,&bpmLabel}) addAndMakeVisible(child);
    editView.setViewedComponent(&timeline, false); trackView.setViewedComponent(&trackPanel, false);
    mixView.setViewedComponent(&mixer, false);
    mixer.onAddAux=[this](bool mono){juce::String error;bool ok=false;audio.editStructure([&]{ok=audio.engine.addAux("Aux "+juce::String(audio.engine.trackCount()+1),mono,error);});if(!ok)showError(error);else{dirty=true;editor.reset();refreshTracks();}};
    mixer.onScanFolder=[this]{chooser=std::make_unique<juce::FileChooser>("Pasta de plugins VST3",juce::File{});chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const auto& c){if(safe && c.getResult().isDirectory())safe->mixer.scanPlugins(juce::FileSearchPath(c.getResult().getFullPathName()));});};
    trackView.setScrollBarsShown(false, false);
    editView.onScroll = [this](auto area) {
        trackView.setViewPosition(0, area.getY()); ruler.scrollX = area.getX(); ruler.repaint();
    };
    trackView.onScroll = [this](auto area) { editView.setViewPosition(editView.getViewPositionX(), area.getY()); };
    trackDivider.onDrag = [this](int delta) { preferences.trackWidth = juce::jlimit(210,420,preferences.trackWidth+delta); resized(); };
    clipDivider.onDrag = [this](int delta) { preferences.clipWidth = juce::jlimit(140,320,preferences.clipWidth-delta); resized(); };
    timeline.selectionChanged = [this] { refreshEdited(); };
    timeline.command = [this](int id) { editCommand(id); };
    clipPanel.selected = [this] {
        if (auto* region = editor.selected()) editView.setViewPosition(static_cast<int>(region->start*timeline.pixelsPerSecond), timeline.rowTop(editor.selectedTrack));
        refreshEdited();
    };
    editButton.onClick = [this] { switchView(false); }; mixButton.onClick = [this] { switchView(true); };
    importButton.onClick = [this] { chooseImport(); };
    playButton.onClick = [this] {
        if (!audio.devices.getCurrentAudioDevice()) { showError(audio.deviceDescription()); return; }
        if (audio.engine.position() >= audio.engine.duration()) audio.engine.seek(0);
        if (audio.engine.trackCount() > 0) audio.engine.play();
    };
    pauseButton.onClick = [this] { audio.engine.pause(); }; stopButton.onClick = [this] { audio.engine.stop(); };
    startButton.onClick = [this] { audio.engine.seek(0); };
    loopButton.setClickingTogglesState(true); snapButton.setClickingTogglesState(true);
    loopButton.onClick = [this] {
        if (loopButton.getToggleState()) setLoopFromSelection();
        else { audio.editStructure([this] { audio.engine.loopEnabled = false; }); dirty = true; refreshEdited(); }
    };
    snapButton.onClick = [this] { editor.snap = snapButton.getToggleState(); preferences.snap = editor.snap; timeline.repaint(); };
    undoButton.onClick = [this] { editCommand(25); }; redoButton.onClick = [this] { editCommand(26); };
    toolLabel.setText("TOOL",juce::dontSendNotification);
    toolBox.addItemList({"Smart","Grabber","Trim","Selector","Split"},1); toolBox.setSelectedId(1);
    toolBox.onChange = [this] { timeline.tool = static_cast<Tool>(toolBox.getSelectedId()-1); };
    gridBox.addItemList({"Grid: 1 beat","Grid: 1/2","Grid: 1/4","Grid: 1/8"},1); gridBox.setSelectedId(1);
    gridBox.onChange = [this] { editor.gridBeats = std::pow(0.5,gridBox.getSelectedId()-1); timeline.repaint(); };
    bpmLabel.setText("BPM",juce::dontSendNotification);
    tempo.setRange(30,300,1); tempo.setValue(120); tempo.setTextBoxStyle(juce::Slider::TextBoxRight,false,45,22);
    tempo.onValueChange = [this] { audio.engine.bpm = tempo.getValue(); dirty = true; timeline.repaint(); ruler.repaint(); };
    signature.addItemList({"4/4","3/4","6/8","7/8","5/4"},1); signature.setSelectedId(1);
    signature.onChange = [this] {
        const auto parts = juce::StringArray::fromTokens(signature.getText(),"/","");
        if (parts.size() == 2) { audio.engine.beatsPerBar = parts[0].getIntValue(); audio.engine.beatUnit = parts[1].getIntValue(); dirty = true; timeline.repaint(); ruler.repaint(); }
    };
    trackHeader.setText("TRACKS",juce::dontSendNotification);
    audioButton.onClick = [this] {
        auto* selector = new juce::AudioDeviceSelectorComponent(audio.devices, 0, 0, 2, 2, false, false, true, false);
        selector->setSize(560, 420); juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(selector); options.dialogTitle = "Dispositivo de audio";
        options.dialogBackgroundColour = panel; options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true; options.resizable = false; options.launchAsync();
    };
    masterLabel.setText("MASTER dB", juce::dontSendNotification);
    master.setRange(-60, 6, 0.1); master.setValue(0); master.setDoubleClickReturnValue(true, 0);
    master.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    master.onValueChange = [this] { audio.engine.masterGain = master.getValue() <= -60 ? 0 : juce::Decibels::decibelsToGain(static_cast<float>(master.getValue())); dirty = true; };
    zoom.setRange(20, 600, 1); zoom.setValue(100); zoom.setTextValueSuffix(" px/s");
    zoom.setTextBoxStyle(juce::Slider::TextBoxRight, false, 75, 20);
    zoom.onValueChange = [this] { preferences.horizontalZoom = zoom.getValue(); timeline.setZoom(zoom.getValue()); ruler.repaint(); };
    verticalZoom.setRange(0.65,2,0.05); verticalZoom.setValue(1); verticalZoom.setTextValueSuffix(" x");
    verticalZoom.setTextBoxStyle(juce::Slider::TextBoxRight,false,50,20);
    verticalZoom.onValueChange = [this] { preferences.verticalZoom = verticalZoom.getValue(); timeline.verticalScale = trackPanel.verticalScale = verticalZoom.getValue(); refreshEdited(); };
    preferencesFile = juce::File(NOVA_PROJECT_ROOT).getChildFile(".nova-preferences.json"); preferences.load(preferencesFile);
    zoom.setValue(preferences.horizontalZoom,juce::dontSendNotification); timeline.setZoom(preferences.horizontalZoom);
    verticalZoom.setValue(preferences.verticalZoom,juce::dontSendNotification); timeline.verticalScale = trackPanel.verticalScale = preferences.verticalZoom;
    editor.snap = preferences.snap; snapButton.setToggleState(editor.snap,juce::dontSendNotification);
    clock.setFont(juce::Font(juce::FontOptions(20).withStyle("Bold")));
    refreshTracks(); switchView(preferences.mix); setSize(1280, 800); startTimerHz(30);
}
MainComponent::~MainComponent() {
    stopTimer();mixer.closeEditors(); if (validationStep < 0) preferences.save(preferencesFile);
    editView.setViewedComponent(nullptr); trackView.setViewedComponent(nullptr); mixView.setViewedComponent(nullptr); setLookAndFeel(nullptr);
}
void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(background); g.setColour(panel); g.fillRect(0, 25, getWidth(), 145);
    g.setColour(accent); g.setFont(juce::Font(juce::FontOptions(22).withStyle("Bold")));
    g.drawText("NOVA", 16, 35, 100, 28, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff94a3b8)); g.setFont(11.0f);
    g.drawText("AUDIO WORKSTATION  /  FASE 3", 17, 64, 225, 20, juce::Justification::centredLeft);
    meter(g, juce::Rectangle<float>(getWidth() - 220.0f, 109, 195, 5), audio.engine.masterPeak.load(), true);
}
void MainComponent::resized() {
    menu.setBounds(0, 0, getWidth(), 25);
    editButton.setBounds(245, 38, 65, 30); mixButton.setBounds(316, 38, 65, 30); importButton.setBounds(397, 38, 130, 30);
    playButton.setBounds(245, 80, 70, 30); pauseButton.setBounds(321, 80, 70, 30); stopButton.setBounds(397, 80, 70, 30);
    clock.setBounds(547, 39, 180, 32); audioButton.setBounds(547, 82, 80, 28);
    startButton.setBounds(16,82,50,28); loopButton.setBounds(72,82,65,28); snapButton.setBounds(143,82,65,28);
    zoom.setBounds(16,128,180,28); verticalZoom.setBounds(200,128,110,28);
    toolLabel.setBounds(320,128,42,28); toolBox.setBounds(364,128,110,28); gridBox.setBounds(480,128,130,28);
    bpmLabel.setBounds(638,83,36,28); tempo.setBounds(674,82,120,28); signature.setBounds(780,128,75,28);
    undoButton.setBounds(624,128,70,28); redoButton.setBounds(700,128,70,28);
    selectionLabel.setBounds(865,128,getWidth()-875,28);
    masterLabel.setBounds(getWidth() - 195, 35, 180, 22); master.setBounds(getWidth() - 195, 63, 180, 36);
    auto content = getLocalBounds().withTrimmedTop(174).withTrimmedBottom(30);
    mixView.setBounds(content); mixer.layoutChannels(mixView.getMaximumVisibleHeight());
    const int tw = std::min(preferences.trackWidth, std::max(210,getWidth()-preferences.clipWidth-380));
    auto tracks = content.removeFromLeft(tw); trackHeader.setBounds(tracks.removeFromTop(48));
    tracks.removeFromBottom(editView.getScrollBarThickness()); trackView.setBounds(tracks);
    trackDivider.setBounds(content.removeFromLeft(5));
    clipPanel.setBounds(content.removeFromRight(preferences.clipWidth)); clipDivider.setBounds(content.removeFromRight(5));
    ruler.setBounds(content.removeFromTop(48)); editView.setBounds(content);
    trackPanel.layoutRows(trackView.getMaximumVisibleWidth()); timeline.updateSize();
    status.setBounds(10, getHeight() - 28, getWidth() - 20, 25);
}
void MainComponent::switchView(bool mix) {
    mixVisible = mix; preferences.mix = mix;
    for (auto* child : std::initializer_list<juce::Component*>{&editView,&trackView,&ruler,&trackHeader,&clipPanel,&trackDivider,&clipDivider}) child->setVisible(!mix);
    mixView.setVisible(mix);
    editButton.setColour(juce::TextButton::buttonColourId, !mix ? juce::Colour(0xff286a62) : line);
    mixButton.setColour(juce::TextButton::buttonColourId, mix ? juce::Colour(0xff286a62) : line);
}
void MainComponent::refreshTracks() {
    auto changed = [this] { dirty = true; };
    trackPanel.rebuild(changed); mixer.rebuild(changed);
    master.setValue(juce::Decibels::gainToDecibels(audio.engine.masterGain.load(), -60.0f), juce::dontSendNotification);
    tempo.setValue(audio.engine.bpm,juce::dontSendNotification);
    const auto timeSignature = juce::String(audio.engine.beatsPerBar)+"/"+juce::String(audio.engine.beatUnit);
    int signatureId = 0;
    for (int i=0;i<signature.getNumItems();++i) if (signature.getItemText(i)==timeSignature) signatureId=signature.getItemId(i);
    if (!signatureId) { signatureId=signature.getNumItems()+1; signature.addItem(timeSignature,signatureId); }
    signature.setSelectedId(signatureId,juce::dontSendNotification);
    refreshEdited(); resized();
}
void MainComponent::refreshEdited() {
    timeline.updateSize(); trackPanel.layoutRows(trackView.getMaximumVisibleWidth());
    trackPanel.refresh(); mixer.refresh(); clipPanel.refresh(); ruler.repaint();
    undoButton.setEnabled(editor.canUndo()); redoButton.setEnabled(editor.canRedo());
    loopButton.setToggleState(audio.engine.loopEnabled.load(),juce::dontSendNotification);
    if (const auto* region=editor.selected()) selectionLabel.setText(region->asset->name+" | "+juce::String(region->start,2)+" - "+juce::String(region->end(),2)+"s",juce::dontSendNotification);
    else selectionLabel.setText("Selecao: "+juce::String(editor.rangeStart,2)+" - "+juce::String(editor.rangeEnd,2)+"s",juce::dontSendNotification);
}
bool MainComponent::applyEdit(std::function<bool()> operation) {
    bool changed=false;
    audio.editStructure([&] {
        changed=operation();
        if (changed) {
            audio.engine.recalculateMediaBytes();
            if (audio.engine.loopEnd.load()>audio.engine.duration()) audio.engine.loopEnabled=false;
            audio.engine.seek(audio.engine.position());
        }
    });
    if (changed) { dirty=true; refreshEdited(); }
    return changed;
}
void MainComponent::editCommand(int id) {
    if (id==20) editor.copy();
    if (id==21) applyEdit([this] { return editor.paste(editor.selectedTrack,editor.quantize(audio.engine.position())); });
    if (id==22) applyEdit([this] { return editor.erase(); });
    if (id==23) applyEdit([this] { return editor.split(editor.quantize(audio.engine.position())); });
    if (id==24) setLoopFromSelection();
    if (id==25) applyEdit([this] { return editor.undo(); });
    if (id==26) applyEdit([this] { return editor.redo(); });
    if (id==27 && editor.copy()) applyEdit([this] { return editor.erase(); });
}
void MainComponent::setLoopFromSelection() {
    double start=editor.rangeStart,end=editor.rangeEnd;
    if (end<=start) if (const auto* region=editor.selected()) { start=region->start; end=region->end(); }
    if (end<=start) { start=0; end=audio.engine.duration(); }
    audio.editStructure([&] { audio.engine.setLoop(start,end,true); });
    dirty=true; refreshEdited();
}
void MainComponent::importFiles(const juce::Array<juce::File>& files) {
    juce::String error;
    for (const auto& file : files) {
        auto clip = AudioEngine::loadWav(file, error);
        if (!clip) { showError(error); continue; }
        bool ok = false;
        audio.editStructure([&] { ok = audio.engine.addClip(std::move(clip), error); });
        if (!ok) showError(error); else dirty = true;
    }
    editor.reset(); refreshTracks();
}
void MainComponent::chooseImport() {
    chooser = std::make_unique<juce::FileChooser>("Importar WAV", juce::File{}, "*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
        [safe = juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& c) { if (safe) safe->importFiles(c.getResults()); });
}
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files) { for (const auto& file : files) if (file.endsWithIgnoreCase(".wav")) return true; return false; }
void MainComponent::filesDropped(const juce::StringArray& files, int, int) {
    juce::Array<juce::File> wavs; for (const auto& file : files) if (file.endsWithIgnoreCase(".wav")) wavs.add(juce::File(file)); importFiles(wavs);
}
void MainComponent::confirmReplacement(std::function<void()> action) {
    if (!dirty) { action(); return; }
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Alteracoes nao salvas").withMessage("Descartar as alteracoes da sessao atual?")
        .withButton("Descartar").withButton("Cancelar"),
        [safe = juce::Component::SafePointer<MainComponent>(this), action](int result) { if (safe && result == 1) action(); });
}
void MainComponent::chooseOpen() {
    confirmReplacement([this] {
        chooser = std::make_unique<juce::FileChooser>("Abrir sessao", juce::File{}, "*.novadaw");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& c) { if (safe && c.getResult().existsAsFile()) safe->openSession(c.getResult()); });
    });
}
void MainComponent::requestClose() {
    confirmReplacement([] { juce::JUCEApplication::getInstance()->quit(); });
}
bool MainComponent::openSession(const juce::File& file) {
    juce::String error; bool ok = false;
    mixer.closeEditors();
    audio.editStructure([&] { ok = Session::load(file, audio.engine, error); });
    if (!ok) { showError(error); return false; }
    editor.reset(); currentSession = file; dirty = false; refreshTracks(); return true;
}
void MainComponent::saveSession(bool saveAs) {
    auto save = [safe = juce::Component::SafePointer<MainComponent>(this)](const juce::File& file) {
        if (!safe) return;
        juce::String error;
        bool ok=false;safe->audio.editStructure([&]{ok=Session::save(file,safe->audio.engine,error);});
        if (ok) { safe->currentSession = file; safe->dirty = false; }
        else safe->showError(error);
    };
    if (!saveAs && currentSession != juce::File{}) { save(currentSession); return; }
    chooser = std::make_unique<juce::FileChooser>("Salvar sessao", currentSession, "*.novadaw");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<MainComponent>(this), save](const juce::FileChooser& c) {
            if (safe && c.getResult() != juce::File{}) save(c.getResult().withFileExtension("novadaw"));
        });
}
void MainComponent::showError(const juce::String& text) {
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "NOVA DAW", text);
}
juce::StringArray MainComponent::getMenuBarNames() { return {"Sessao", "Editar", "Transporte", "Janela"}; }
juce::PopupMenu MainComponent::getMenuForIndex(int index, const juce::String&) {
    juce::PopupMenu p;
    if (index == 0) { p.addItem(1,"Nova sessao"); p.addItem(2,"Abrir... (Ctrl+O)"); p.addItem(3,"Salvar (Ctrl+S)"); p.addItem(4,"Salvar como..."); p.addSeparator(); p.addItem(5,"Importar WAV... (Ctrl+I)"); }
    if (index == 1) {
        p.addItem(25,"Undo (Ctrl+Z)",editor.canUndo()); p.addItem(26,"Redo (Ctrl+Shift+Z)",editor.canRedo()); p.addSeparator();
        p.addItem(20,"Copy (Ctrl+C)",editor.selected()!=nullptr); p.addItem(27,"Cut (Ctrl+X)",editor.selected()!=nullptr);
        p.addItem(21,"Paste (Ctrl+V)",editor.canPaste() && editor.selectedTrack>=0); p.addItem(22,"Delete (Del)",editor.selected()!=nullptr);
        p.addItem(23,"Split (Ctrl+E)",editor.selected()!=nullptr); p.addItem(24,"Loop selection",audio.engine.duration()>0);
    }
    if (index == 2) { p.addItem(6,"Play / Pause (Espaco)"); p.addItem(7,"Stop (Home)"); }
    if (index == 3) { p.addItem(8,"Edit",true,!mixVisible); p.addItem(9,"Mix",true,mixVisible); p.addItem(10,"Dispositivo de audio..."); }
    return p;
}
void MainComponent::menuItemSelected(int id, int) {
    if (id >=20 && id<=27) { editCommand(id); return; }
    if (id == 1) confirmReplacement([this] { mixer.closeEditors(); audio.editStructure([this] { audio.engine.clear(); }); editor.reset(); currentSession = {}; dirty = false; refreshTracks(); });
    if (id == 2) chooseOpen(); if (id == 3) saveSession(false); if (id == 4) saveSession(true); if (id == 5) chooseImport();
    if (id == 6) { if (audio.engine.isPlaying()) pauseButton.onClick(); else playButton.onClick(); }
    if (id == 7) stopButton.onClick(); if (id == 8) switchView(false); if (id == 9) switchView(true); if (id == 10) audioButton.onClick();
}
bool MainComponent::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::spaceKey) { menuItemSelected(6,0); return true; }
    if (key == juce::KeyPress::homeKey) { menuItemSelected(7,0); return true; }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { editCommand(22); return true; }
    if (key.getModifiers().isCommandDown()) {
        if (key.getKeyCode() == 'Z') { editCommand(key.getModifiers().isShiftDown()?26:25); return true; }
        if (key.getKeyCode() == 'Y') { editCommand(26); return true; }
        if (key.getKeyCode() == 'C') { editCommand(20); return true; }
        if (key.getKeyCode() == 'X') { editCommand(27); return true; }
        if (key.getKeyCode() == 'V') { editCommand(21); return true; }
        if (key.getKeyCode() == 'E') { editCommand(23); return true; }
        if (key.getKeyCode() == 'I') { chooseImport(); return true; }
        if (key.getKeyCode() == 'O') { chooseOpen(); return true; }
        if (key.getKeyCode() == 'S') { saveSession(key.getModifiers().isShiftDown()); return true; }
    }
    return false;
}
void MainComponent::enableUiValidation(const juce::File& dir) { validationDirectory = dir; validationStep = 0; }
bool MainComponent::validateEditor() {
    if (audio.engine.trackCount()<2) return false;
    zoom.setValue(100,juce::sendNotificationSync); verticalZoom.setValue(1,juce::sendNotificationSync);
    snapButton.setToggleState(true,juce::dontSendNotification); snapButton.onClick(); switchView(false);
    const auto original=audio.engine.track(0).regions.front().id;
    auto event=[this](float x,float y) {
        const auto now=juce::Time::getCurrentTime();
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),{x,y},juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&timeline,&timeline,now,{x,y},now,1,true);
    };
    toolBox.setSelectedId(2,juce::sendNotificationSync);
    timeline.mouseDown(event(100,50)); timeline.mouseDrag(event(200,50)); timeline.mouseUp(event(200,50));
    bool ok=editor.selectedRegion==original && editor.selected() && editor.selected()->start==1;
    keyPressed(juce::KeyPress('Z',juce::ModifierKeys::ctrlModifier,0));
    ok=ok && editor.selected() && editor.selected()->start==0;
    keyPressed(juce::KeyPress('Z',juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier,0));
    ok=ok && editor.selected() && editor.selected()->start==1;
    toolBox.setSelectedId(3,juce::sendNotificationSync);
    timeline.mouseDown(event(101,50)); timeline.mouseDrag(event(151,50)); timeline.mouseUp(event(151,50));
    ok=ok && editor.selected() && editor.selected()->start==1.5 && editor.selected()->offset==0.5;
    toolBox.setSelectedId(5,juce::sendNotificationSync); timeline.mouseDown(event(300,50));
    ok=ok && audio.engine.track(0).regions.size()==2 && editor.selected() && editor.selected()->length==1.5;
    keyPressed(juce::KeyPress('C',juce::ModifierKeys::ctrlModifier,0));
    editor.select(1,0); audio.engine.seek(0);
    keyPressed(juce::KeyPress('V',juce::ModifierKeys::ctrlModifier,0));
    ok=ok && audio.engine.track(1).regions.size()==2;
    keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
    ok=ok && audio.engine.track(1).regions.size()==1;
    undoButton.onClick(); ok=ok && audio.engine.track(1).regions.size()==2;
    redoButton.onClick(); ok=ok && audio.engine.track(1).regions.size()==1;
    toolBox.setSelectedId(4,juce::sendNotificationSync);
    timeline.mouseDown(event(150,50)); timeline.mouseDrag(event(300,50)); timeline.mouseUp(event(300,50));
    loopButton.setToggleState(true,juce::dontSendNotification); loopButton.onClick();
    ok=ok && audio.engine.loopEnabled && audio.engine.loopStart==1.5 && audio.engine.loopEnd==3;
    loopButton.setToggleState(false,juce::dontSendNotification); loopButton.onClick();
    const int width=getWidth(),height=getHeight(); setSize(1000,650);
    ok=ok && ruler.getWidth()>300 && editView.getWidth()>300 && clipPanel.getRight()<=getWidth();
    setSize(width,height); editView.setViewPosition(120,80);
    ok=ok && trackView.getViewPositionY()==editView.getViewPositionY() && ruler.scrollX==editView.getViewPositionX();
    editView.setViewPosition(0,0); toolBox.setSelectedId(1,juce::sendNotificationSync); audio.engine.stop();
    juce::String error; const auto saved=validationDirectory.getChildFile("edited-ui.novadaw");
    bool savedOk=false;audio.editStructure([&]{savedOk=Session::save(saved,audio.engine,error);});ok=ok && savedOk;
    if (ok) ok=openSession(saved) && audio.engine.track(0).regions.size()==2;
    editor.select(0, audio.engine.track(0).regions.front().id); refreshEdited();
    return ok;
}
void MainComponent::timerCallback() {
    const double seconds = audio.engine.position();
    clock.setText(juce::String(static_cast<int>(seconds)/60).paddedLeft('0',2) + ":"
        + juce::String(std::fmod(seconds,60.0),2).paddedLeft('0',5), juce::dontSendNotification);
    status.setText((dirty ? "* " : "") + (currentSession == juce::File{} ? "Sessao sem nome" : currentSession.getFileName())
        + " | " + juce::String(audio.engine.trackCount()) + " faixas | " + audio.deviceDescription(), juce::dontSendNotification);
    trackPanel.refresh(); mixer.refresh(); ruler.repaint(); timeline.repaint(); repaint();
    master.setValue(juce::Decibels::gainToDecibels(audio.engine.masterGain.load(),-60.0f),juce::dontSendNotification);
    if (validationStep < 0 || ++callbackTicks % 15 != 0) return;
    if (validationStep == 0) {
        juce::Array<juce::File> sources;
        for (int i = 0; i < audio.engine.trackCount(); ++i) if(!audio.engine.track(i).auxiliary) sources.add(audio.engine.track(i).clip->source);
        mixer.closeEditors(); audio.editStructure([this] { audio.engine.clear(); });
        importFiles(sources);
        validationImported = audio.engine.trackCount() >= 2;
        const bool editBindings=trackPanel.validateBindings(),mixBindings=mixer.validateBindings();
        validationControls=editBindings&&mixBindings;
        validationDirectory.createDirectory();validationDirectory.getChildFile("bindings.txt").replaceWithText("Edit="+juce::String(int(editBindings))+" Mix="+juce::String(int(mixBindings)));
        validationDirectory.createDirectory();
        validationEditing = validateEditor();
        juce::String auxiliaryError;audio.editStructure([&]{audio.engine.addAux("Validation FX",false,auxiliaryError);});editor.reset();refreshTracks();
        validationPhase3=mixer.validatePhase3Bindings();dirty=false;
        juce::String saveError;audio.editStructure([&]{Session::save(validationDirectory.getChildFile("phase3-ui.novadaw"),audio.engine,saveError);});
        auto image = createComponentSnapshot(getLocalBounds());
        if (auto stream = validationDirectory.getChildFile("edit.png").createOutputStream()) {
            stream->setPosition(0); stream->truncate(); juce::PNGImageFormat().writeImageToStream(image,*stream);
        }
        playButton.onClick();
    } else if (validationStep == 1) {
        validationSawPlay = audio.engine.position() > 0.1;
        validationSawAudio = audio.engine.masterPeak.load() > 0;
        pauseButton.onClick(); validationPausePosition = audio.engine.position();
    } else if (validationStep == 2) {
        validationSawPause = !audio.engine.isPlaying() && std::abs(audio.engine.position()-validationPausePosition) < 0.05;
        mixButton.onClick();
        auto image = createComponentSnapshot(getLocalBounds());
        if (auto stream = validationDirectory.getChildFile("mix.png").createOutputStream()) {
            stream->setPosition(0); stream->truncate(); juce::PNGImageFormat().writeImageToStream(image,*stream);
        }
        playButton.onClick();
    } else if (validationStep == 3) {
        validationResume = audio.engine.position() > validationPausePosition + 0.1;
        audio.editStructure([this] { audio.engine.setLoop(0.1,0.2,true); audio.engine.seek(0.15); });
        playButton.onClick();
    } else if (validationStep == 4) {
        validationLoop = audio.engine.isPlaying() && audio.engine.position()>=0.1 && audio.engine.position()<0.2 && audio.engine.masterPeak.load()>0;
        stopButton.onClick();
    } else if (validationStep == 5) {
        const bool ok = validationPhase3 && validationEditing && validationLoop && validationControls && validationImported && validationResume && validationSawPlay && validationSawAudio && validationSawPause
            && !audio.engine.isPlaying() && audio.engine.position() == 0 && audio.engine.trackCount() >= 2;
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("phase3NativeEditorSendAutomationMaster",validationPhase3);root->setProperty("passed",ok); root->setProperty("play",validationSawPlay); root->setProperty("hardwareCallbackNonzero",validationSawAudio);
        root->setProperty("wavImport",validationImported); root->setProperty("resume",validationResume);
        root->setProperty("editMixControlBindings",validationControls);
        root->setProperty("editingGesturesShortcutsLayoutSession",validationEditing); root->setProperty("hardwareLoop",validationLoop);
        root->setProperty("pause",validationSawPause); root->setProperty("stop",audio.engine.position()==0);
        root->setProperty("tracks",audio.engine.trackCount()); root->setProperty("device",audio.deviceDescription());
        validationDirectory.getChildFile("ui-result.json").replaceWithText(juce::JSON::toString(juce::var(root.release()),false));
        juce::JUCEApplication::getInstance()->setApplicationReturnValue(ok ? 0 : 1);
        juce::JUCEApplication::getInstance()->quit();
    }
    ++validationStep;
}
}
