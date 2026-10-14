#include "MainComponent.h"
#include <algorithm>
#include <cmath>

namespace nova {
namespace {
const juce::Colour background{0xff12161e}, panel{0xff1b222e}, line{0xff30394a}, accent{0xff55d8c2};
juce::Colour trackColour(int index) { return juce::Colour::fromHSV(std::fmod(0.47f + index * 0.13f, 1.0f), 0.5f, 0.88f, 1.0f); }
void meter(juce::Graphics& g, juce::Rectangle<float> bounds, float level, bool horizontal) {
    g.setColour(juce::Colour(0xff090c12)); g.fillRoundedRectangle(bounds, 2);
    g.setColour(level > 1 ? juce::Colour(0xffff6d73) : accent);
    const float amount = juce::jlimit(0.0f, 1.0f, level);
    if (horizontal) g.fillRect(bounds.withWidth(bounds.getWidth() * amount));
    else g.fillRect(bounds.removeFromBottom(bounds.getHeight() * amount));
}
}
TrackControls::TrackControls(AudioEngine& e, int i, bool v, std::function<void()> changed)
    : engine(e), index(i), vertical(v) {
    for (auto* child : std::initializer_list<juce::Component*>{&name, &gainLabel, &panLabel, &gain, &pan, &mute, &solo}) addAndMakeVisible(child);
    name.setText(engine.track(index).clip->name, juce::dontSendNotification);
    name.setColour(juce::Label::textColourId, trackColour(index));
    name.setFont(juce::Font(juce::FontOptions(14).withStyle("Bold")));
    gainLabel.setText("Volume dB", juce::dontSendNotification); panLabel.setText("Pan", juce::dontSendNotification);
    gain.setRange(-60, 6, 0.1); gain.setDoubleClickReturnValue(true, 0);
    gain.setSliderStyle(v ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
    gain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
    pan.setRange(-1, 1, 0.01); pan.setDoubleClickReturnValue(true, 0);
    pan.setSliderStyle(v ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal);
    pan.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
    mute.setClickingTogglesState(true); solo.setClickingTogglesState(true);
    mute.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffae555d));
    solo.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffaf8b35));
    gain.onValueChange = [this, changed] { engine.track(index).gain = gain.getValue() <= -60 ? 0 : juce::Decibels::decibelsToGain(static_cast<float>(gain.getValue())); changed(); };
    pan.onValueChange = [this, changed] { engine.track(index).pan = static_cast<float>(pan.getValue()); changed(); };
    mute.onClick = [this, changed] { engine.track(index).mute = mute.getToggleState(); changed(); };
    solo.onClick = [this, changed] { engine.track(index).solo = solo.getToggleState(); changed(); };
    refresh();
}
void TrackControls::refresh() {
    auto& t = engine.track(index);
    gain.setValue(juce::Decibels::gainToDecibels(t.gain.load(), -60.0f), juce::dontSendNotification);
    pan.setValue(t.pan.load(), juce::dontSendNotification);
    mute.setToggleState(t.mute.load(), juce::dontSendNotification); solo.setToggleState(t.solo.load(), juce::dontSendNotification);
    repaint();
}
bool TrackControls::validateBindings() {
    auto& t = engine.track(index);
    const float oldGain=t.gain.load(), oldPan=t.pan.load();
    const bool oldMute=t.mute.load(), oldSolo=t.solo.load();
    gain.setValue(-6,juce::sendNotificationSync); pan.setValue(1,juce::sendNotificationSync);
    mute.setToggleState(true,juce::sendNotificationSync); solo.setToggleState(true,juce::sendNotificationSync);
    const bool ok = std::abs(t.gain.load()-juce::Decibels::decibelsToGain(-6.0f))<0.00001f
        && t.pan.load()==1 && t.mute.load() && t.solo.load();
    t.gain=oldGain; t.pan=oldPan; t.mute=oldMute; t.solo=oldSolo; refresh(); return ok;
}
void TrackControls::paint(juce::Graphics& g) {
    g.fillAll(panel); g.setColour(line); g.drawRect(getLocalBounds());
    const float peak = engine.track(index).peak.load();
    meter(g, vertical ? juce::Rectangle<float>(getWidth() - 20.0f, 130, 7, getHeight() - 150.0f)
                      : juce::Rectangle<float>(10, getHeight() - 9.0f, getWidth() - 20.0f, 4), peak, !vertical);
}
void TrackControls::resized() {
    name.setBounds(10, 8, getWidth() - 20, 25);
    mute.setBounds(10, 38, 42, 26); solo.setBounds(58, 38, 42, 26);
    if (vertical) {
        panLabel.setBounds(10, 72, 75, 18); pan.setBounds(10, 90, getWidth() - 32, 80);
        gainLabel.setBounds(10, 175, 100, 18); gain.setBounds(10, 194, getWidth() - 40, getHeight() - 212);
    } else {
        gainLabel.setBounds(110, 34, 100, 18); gain.setBounds(105, 52, getWidth() - 115, 43);
        panLabel.setBounds(10, 72, 65, 18); pan.setBounds(5, 87, getWidth() - 15, 44);
    }
}
Timeline::Timeline(AudioEngine& e) : engine(e) {}
bool Timeline::validateBindings() { return !controls.empty() && controls.front()->validateBindings(); }
void Timeline::rebuild(std::function<void()> changed) {
    controls.clear();
    for (int i = 0; i < engine.trackCount(); ++i) {
        auto control = std::make_unique<TrackControls>(engine, i, false, changed);
        addAndMakeVisible(*control); control->setBounds(0, 48 + i * 144, 238, 144); controls.push_back(std::move(control));
    }
    setZoom(pixelsPerSecond);
}
void Timeline::setZoom(double value) {
    pixelsPerSecond = value;
    setSize(std::max(1100, 240 + static_cast<int>(std::min(1000000.0, std::max(12.0, engine.duration()) * value))),
            std::max(400, 48 + engine.trackCount() * 144)); repaint();
}
void Timeline::paint(juce::Graphics& g) {
    g.fillAll(background); g.setColour(panel); g.fillRect(0, 0, getWidth(), 48);
    g.setColour(juce::Colours::white); g.setFont(13.0f); g.drawText("TRACKS", 14, 0, 200, 46, juce::Justification::centredLeft);
    const auto clipBounds = g.getClipBounds();
    const int interval = std::max(1, static_cast<int>(std::ceil(65.0 / pixelsPerSecond)));
    const int first = std::max(0, static_cast<int>((clipBounds.getX() - 240) / pixelsPerSecond) / interval * interval);
    const int last = std::max(0, static_cast<int>((clipBounds.getRight() - 240) / pixelsPerSecond) + interval);
    for (int second = first; second <= last; second += interval) {
        const int x = 240 + static_cast<int>(second * pixelsPerSecond);
        g.setColour(line); g.drawVerticalLine(x, 48.0f, static_cast<float>(getHeight()));
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText(juce::String(second / 60).paddedLeft('0', 2) + ":" + juce::String(second % 60).paddedLeft('0', 2), x + 4, 10, 60, 25, juce::Justification::centredLeft);
    }
    for (int i = 0; i < engine.trackCount(); ++i) {
        const auto& clip = *engine.track(i).clip;
        const int y = 48 + i * 144;
        g.setColour(line); g.drawHorizontalLine(y + 143, 0, static_cast<float>(getWidth()));
        juce::Rectangle<float> box(242, y + 12.0f, static_cast<float>(clip.duration() * pixelsPerSecond), 116);
        const auto colour = trackColour(i);
        g.setColour(colour.withAlpha(0.18f)); g.fillRoundedRectangle(box, 4);
        g.setColour(colour.withAlpha(0.6f)); g.drawRoundedRectangle(box, 4, 1);
        g.setColour(colour); g.drawText(clip.name, box.toNearestInt().reduced(8).removeFromTop(22), juce::Justification::centredLeft);
        const int left = std::max(static_cast<int>(box.getX()), clipBounds.getX());
        const int right = std::min(static_cast<int>(box.getRight()), clipBounds.getRight());
        const float centre = y + 84.0f;
        for (int x = left; x < right; ++x) {
            const auto bin = std::min(clip.waveform.size() - 1, static_cast<size_t>(std::max(0.0f, (x - box.getX()) / box.getWidth()) * clip.waveform.size()));
            const auto peak = clip.waveform[bin];
            g.drawVerticalLine(x, centre - peak.high * 32, centre - peak.low * 32 + 1);
        }
    }
    if (engine.trackCount() == 0) {
        g.setColour(juce::Colour(0xff94a3b8)); g.setFont(19.0f);
        g.drawText("Importe WAV ou arraste arquivos para criar faixas", 280, 130, 720, 60, juce::Justification::centred);
    }
    g.setColour(accent); g.drawVerticalLine(240 + static_cast<int>(engine.position() * pixelsPerSecond), 0, static_cast<float>(getHeight()));
    for (auto& control : controls) control->refresh();
}
void Timeline::mouseDown(const juce::MouseEvent& event) { if (event.x >= 240) engine.seek((event.x - 240) / pixelsPerSecond); }
Mixer::Mixer(AudioEngine& e) : engine(e) {}
bool Mixer::validateBindings() { return !controls.empty() && controls.front()->validateBindings(); }
void Mixer::rebuild(std::function<void()> changed) {
    controls.clear();
    for (int i = 0; i < engine.trackCount(); ++i) {
        auto control = std::make_unique<TrackControls>(engine, i, true, changed);
        addAndMakeVisible(*control); control->setBounds(12 + i * 150, 48, 140, 470); controls.push_back(std::move(control));
    }
    setSize(std::max(1100, 24 + engine.trackCount() * 150), 540); repaint();
}
void Mixer::paint(juce::Graphics& g) {
    g.fillAll(background); g.setColour(juce::Colours::white); g.setFont(16.0f);
    g.drawText("MIXER  /  Volume, pan, solo e mute", 16, 8, 600, 32, juce::Justification::centredLeft);
    for (auto& control : controls) control->refresh();
}
MainComponent::MainComponent() : timeline(audio.engine), mixer(audio.engine), menu(this) {
    look.setColour(juce::ResizableWindow::backgroundColourId, background);
    look.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff293444));
    look.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    look.setColour(juce::Slider::thumbColourId, accent); look.setColour(juce::Slider::trackColourId, accent);
    look.setColour(juce::Label::textColourId, juce::Colour(0xffd4deeb));
    look.setColour(juce::PopupMenu::backgroundColourId, panel);
    setLookAndFeel(&look); setWantsKeyboardFocus(true);
    for (auto* child : std::initializer_list<juce::Component*>{&menu,&editButton,&mixButton,&importButton,&playButton,&pauseButton,&stopButton,&audioButton,&clock,&status,&masterLabel,&master,&zoom,&editView,&mixView}) addAndMakeVisible(child);
    editView.setViewedComponent(&timeline, false); mixView.setViewedComponent(&mixer, false);
    editButton.onClick = [this] { switchView(false); }; mixButton.onClick = [this] { switchView(true); };
    importButton.onClick = [this] { chooseImport(); };
    playButton.onClick = [this] {
        if (!audio.devices.getCurrentAudioDevice()) { showError(audio.deviceDescription()); return; }
        if (audio.engine.position() >= audio.engine.duration()) audio.engine.seek(0);
        if (audio.engine.trackCount() > 0) audio.engine.play();
    };
    pauseButton.onClick = [this] { audio.engine.pause(); }; stopButton.onClick = [this] { audio.engine.stop(); };
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
    zoom.setRange(20, 300, 1); zoom.setValue(90); zoom.setTextValueSuffix(" px/s");
    zoom.setTextBoxStyle(juce::Slider::TextBoxRight, false, 75, 20); zoom.onValueChange = [this] { timeline.setZoom(zoom.getValue()); };
    clock.setFont(juce::Font(juce::FontOptions(20).withStyle("Bold")));
    refreshTracks(); switchView(false); setSize(1280, 800); startTimerHz(30);
}
MainComponent::~MainComponent() { stopTimer(); editView.setViewedComponent(nullptr); mixView.setViewedComponent(nullptr); setLookAndFeel(nullptr); }
void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(background); g.setColour(panel); g.fillRect(0, 25, getWidth(), 98);
    g.setColour(accent); g.setFont(juce::Font(juce::FontOptions(22).withStyle("Bold")));
    g.drawText("NOVA", 16, 35, 100, 28, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff94a3b8)); g.setFont(11.0f);
    g.drawText("AUDIO WORKSTATION  /  FASE 1", 17, 64, 225, 20, juce::Justification::centredLeft);
    meter(g, juce::Rectangle<float>(getWidth() - 220.0f, 109, 195, 5), audio.engine.masterPeak.load(), true);
}
void MainComponent::resized() {
    menu.setBounds(0, 0, getWidth(), 25);
    editButton.setBounds(245, 38, 65, 30); mixButton.setBounds(316, 38, 65, 30); importButton.setBounds(397, 38, 130, 30);
    playButton.setBounds(245, 80, 70, 30); pauseButton.setBounds(321, 80, 70, 30); stopButton.setBounds(397, 80, 70, 30);
    clock.setBounds(547, 39, 180, 32); audioButton.setBounds(547, 82, 80, 28); zoom.setBounds(646, 81, 185, 30);
    masterLabel.setBounds(getWidth() - 220, 35, 195, 22); master.setBounds(getWidth() - 230, 63, 210, 36);
    auto content = getLocalBounds().withTrimmedTop(124).withTrimmedBottom(30);
    editView.setBounds(content); mixView.setBounds(content);
    status.setBounds(10, getHeight() - 28, getWidth() - 20, 25);
}
void MainComponent::switchView(bool mix) {
    mixVisible = mix; editView.setVisible(!mix); mixView.setVisible(mix);
    editButton.setColour(juce::TextButton::buttonColourId, !mix ? juce::Colour(0xff286a62) : line);
    mixButton.setColour(juce::TextButton::buttonColourId, mix ? juce::Colour(0xff286a62) : line);
}
void MainComponent::refreshTracks() {
    auto changed = [this] { dirty = true; };
    timeline.rebuild(changed); mixer.rebuild(changed);
    master.setValue(juce::Decibels::gainToDecibels(audio.engine.masterGain.load(), -60.0f), juce::dontSendNotification);
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
    refreshTracks();
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
    audio.editStructure([&] { ok = Session::load(file, audio.engine, error); });
    if (!ok) { showError(error); return false; }
    currentSession = file; dirty = false; refreshTracks(); return true;
}
void MainComponent::saveSession(bool saveAs) {
    auto save = [safe = juce::Component::SafePointer<MainComponent>(this)](const juce::File& file) {
        if (!safe) return;
        juce::String error;
        if (Session::save(file, safe->audio.engine, error)) { safe->currentSession = file; safe->dirty = false; }
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
juce::StringArray MainComponent::getMenuBarNames() { return {"Sessao", "Transporte", "Janela"}; }
juce::PopupMenu MainComponent::getMenuForIndex(int index, const juce::String&) {
    juce::PopupMenu p;
    if (index == 0) { p.addItem(1,"Nova sessao"); p.addItem(2,"Abrir... (Ctrl+O)"); p.addItem(3,"Salvar (Ctrl+S)"); p.addItem(4,"Salvar como..."); p.addSeparator(); p.addItem(5,"Importar WAV... (Ctrl+I)"); }
    if (index == 1) { p.addItem(6,"Play / Pause (Espaco)"); p.addItem(7,"Stop (Home)"); }
    if (index == 2) { p.addItem(8,"Edit",true,!mixVisible); p.addItem(9,"Mix",true,mixVisible); p.addItem(10,"Dispositivo de audio..."); }
    return p;
}
void MainComponent::menuItemSelected(int id, int) {
    if (id == 1) confirmReplacement([this] { audio.editStructure([this] { audio.engine.clear(); }); currentSession = {}; dirty = false; refreshTracks(); });
    if (id == 2) chooseOpen(); if (id == 3) saveSession(false); if (id == 4) saveSession(true); if (id == 5) chooseImport();
    if (id == 6) { if (audio.engine.isPlaying()) pauseButton.onClick(); else playButton.onClick(); }
    if (id == 7) stopButton.onClick(); if (id == 8) switchView(false); if (id == 9) switchView(true); if (id == 10) audioButton.onClick();
}
bool MainComponent::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::spaceKey) { menuItemSelected(6,0); return true; }
    if (key == juce::KeyPress::homeKey) { menuItemSelected(7,0); return true; }
    if (key.getModifiers().isCommandDown()) {
        if (key.getKeyCode() == 'I') { chooseImport(); return true; }
        if (key.getKeyCode() == 'O') { chooseOpen(); return true; }
        if (key.getKeyCode() == 'S') { saveSession(key.getModifiers().isShiftDown()); return true; }
    }
    return false;
}
void MainComponent::enableUiValidation(const juce::File& dir) { validationDirectory = dir; validationStep = 0; }
void MainComponent::timerCallback() {
    const double seconds = audio.engine.position();
    clock.setText(juce::String(static_cast<int>(seconds)/60).paddedLeft('0',2) + ":"
        + juce::String(std::fmod(seconds,60.0),2).paddedLeft('0',5), juce::dontSendNotification);
    status.setText((dirty ? "* " : "") + (currentSession == juce::File{} ? "Sessao sem nome" : currentSession.getFileName())
        + " | " + juce::String(audio.engine.trackCount()) + " faixas | " + audio.deviceDescription(), juce::dontSendNotification);
    timeline.repaint(); mixer.repaint(); repaint();
    if (validationStep < 0 || ++callbackTicks % 15 != 0) return;
    if (validationStep == 0) {
        juce::Array<juce::File> sources;
        for (int i = 0; i < audio.engine.trackCount(); ++i) sources.add(audio.engine.track(i).clip->source);
        audio.editStructure([this] { audio.engine.clear(); });
        importFiles(sources);
        validationImported = audio.engine.trackCount() >= 2;
        validationControls = timeline.validateBindings() && mixer.validateBindings();
        dirty = false;
        validationDirectory.createDirectory();
        auto image = createComponentSnapshot(getLocalBounds());
        if (auto stream = validationDirectory.getChildFile("edit.png").createOutputStream()) juce::PNGImageFormat().writeImageToStream(image,*stream);
        playButton.onClick();
    } else if (validationStep == 1) {
        validationSawPlay = audio.engine.position() > 0.1;
        validationSawAudio = audio.engine.masterPeak.load() > 0;
        pauseButton.onClick(); validationPausePosition = audio.engine.position();
    } else if (validationStep == 2) {
        validationSawPause = !audio.engine.isPlaying() && std::abs(audio.engine.position()-validationPausePosition) < 0.05;
        mixButton.onClick();
        auto image = createComponentSnapshot(getLocalBounds());
        if (auto stream = validationDirectory.getChildFile("mix.png").createOutputStream()) juce::PNGImageFormat().writeImageToStream(image,*stream);
        playButton.onClick();
    } else if (validationStep == 3) {
        validationResume = audio.engine.position() > validationPausePosition + 0.1;
        stopButton.onClick();
    } else if (validationStep == 4) {
        const bool ok = validationControls && validationImported && validationResume && validationSawPlay && validationSawAudio && validationSawPause
            && !audio.engine.isPlaying() && audio.engine.position() == 0 && audio.engine.trackCount() >= 2;
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("passed",ok); root->setProperty("play",validationSawPlay); root->setProperty("hardwareCallbackNonzero",validationSawAudio);
        root->setProperty("wavImport",validationImported); root->setProperty("resume",validationResume);
        root->setProperty("editMixControlBindings",validationControls);
        root->setProperty("pause",validationSawPause); root->setProperty("stop",audio.engine.position()==0);
        root->setProperty("tracks",audio.engine.trackCount()); root->setProperty("device",audio.deviceDescription());
        validationDirectory.getChildFile("ui-result.json").replaceWithText(juce::JSON::toString(juce::var(root.release()),false));
        juce::JUCEApplication::getInstance()->setApplicationReturnValue(ok ? 0 : 1);
        juce::JUCEApplication::getInstance()->quit();
    }
    ++validationStep;
}
}
