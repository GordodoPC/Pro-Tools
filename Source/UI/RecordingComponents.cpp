#include "RecordingComponents.h"
#include "Audio/AudioRecorder.h"
namespace nova {
namespace {
class RecordingPanel final : public juce::Component,private juce::Timer {
public:
    RecordingPanel(AudioRuntime& owner,int selected,juce::File target,std::function<void()> notify):runtime(owner),engine(owner.engine),folder(std::move(target)),changed(std::move(notify)){
        for(auto* component:std::initializer_list<juce::Component*>{&tracks,&left,&right,&stereo,&arm,&monitor,&gain,&punch,&loop,&punchIn,&punchOut,&preRoll,&postRoll,&loopStart,&loopEnd,&start,&stop,&takes,&activate,&recover,&status})addAndMakeVisible(component);
        for(int i=0;i<engine.trackCount();++i)if(!engine.track(i).midiTrack&&!engine.track(i).auxiliary)tracks.addItem(engine.track(i).name,i+1);
        tracks.setSelectedId(selected+1,juce::dontSendNotification);if(!tracks.getSelectedId()&&tracks.getNumItems()>0)tracks.setSelectedItemIndex(0,juce::dontSendNotification);track=tracks.getSelectedId()-1;
        if(auto* device=runtime.devices.getCurrentAudioDevice()){const auto names=device->getInputChannelNames();const auto active=device->getActiveInputChannels();int logical=0;
            for(int i=0;i<names.size();++i)if(active[i]){left.addItem(names[i],logical+1);right.addItem(names[i],logical+1);++logical;}}
        if(left.getNumItems()==0){left.addItem("Ative entradas em Audio",1);right.addItem("Ative entradas em Audio",1);}
        for(auto* button:{&stereo,&arm,&monitor,&punch,&loop})button->setClickingTogglesState(true);
        tracks.onChange=[this]{track=tracks.getSelectedId()-1;refresh();};left.onChange=[this]{configure();};right.onChange=[this]{configure();};stereo.onClick=[this]{configure();};
        arm.onClick=[this]{if(valid()){engine.track(track).recordArm=arm.getToggleState();changed();}};
        monitor.setTooltip("Monitoramento opt-in: use fones e evite microfone perto dos alto-falantes.");monitor.onClick=[this]{if(valid()){if(monitor.getToggleState())engine.monitoringAllowed=true;engine.track(track).monitorPreference=monitor.getToggleState();engine.track(track).monitorInput=monitor.getToggleState();changed();}};
        gain.setRange(-24,24,0.1);gain.setTextValueSuffix(" dB de captura");gain.setTextBoxStyle(juce::Slider::TextBoxRight,false,145,24);gain.onValueChange=[this]{if(valid()){engine.track(track).recordGain=juce::Decibels::decibelsToGain(float(gain.getValue()));changed();}};
        const auto& options=engine.recordingOptions;punch.setToggleState(options.punch,juce::dontSendNotification);loop.setToggleState(options.loop,juce::dontSendNotification);punchIn.setText(juce::String(options.punchIn,3));punchOut.setText(juce::String(options.punchOut,3));preRoll.setText(juce::String(options.preRoll,3));postRoll.setText(juce::String(options.postRoll,3));loopStart.setText(juce::String(options.loopStart,3));loopEnd.setText(juce::String(options.loopEnd,3));
        for(auto* text:{&punchIn,&punchOut,&preRoll,&postRoll,&loopStart,&loopEnd})text->setInputRestrictions(16,"0123456789.");
        start.onClick=[this]{RecordingOptions options;options.punch=punch.getToggleState();options.loop=loop.getToggleState();options.punchIn=punchIn.getText().getDoubleValue();options.punchOut=punchOut.getText().getDoubleValue();options.preRoll=preRoll.getText().getDoubleValue();options.postRoll=postRoll.getText().getDoubleValue();options.loopStart=loopStart.getText().getDoubleValue();options.loopEnd=loopEnd.getText().getDoubleValue();
            int inputs=0;if(auto* device=runtime.devices.getCurrentAudioDevice())inputs=device->getActiveInputChannels().countNumberOfSetBits();juce::String error;if(!runtime.audioRecorder->start(folder,inputs,options,error))status.setText(error,juce::dontSendNotification);else{changed();status.setText(runtime.audioRecorder->status(),juce::dontSendNotification);}};
        stop.onClick=[this]{juce::String error;runtime.audioRecorder->stop(error);engine.stop();changed();refresh();status.setText(error.isEmpty()?runtime.audioRecorder->status():error,juce::dontSendNotification);};
        activate.onClick=[this]{if(!valid())return;const int selected=takes.getSelectedId()-1;if(selected<0||selected>=int(engine.track(track).takes.size()))return;juce::String error;if(engine.activateTake(track,engine.track(track).takes[selected].id,error)){changed();refresh();}else status.setText(error,juce::dontSendNotification);};
        recover.onClick=[this]{chooser=std::make_unique<juce::FileChooser>("Pasta capture-* de recuperacao",folder);chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,
            [safe=juce::Component::SafePointer<RecordingPanel>(this)](const auto& chosen){if(!safe||!chosen.getResult().isDirectory())return;juce::String error;if(safe->runtime.audioRecorder->recover(chosen.getResult(),error)){safe->changed();safe->refresh();}else safe->status.setText(error,juce::dontSendNotification);});};
        refresh();setSize(640,550);startTimerHz(10);
    }
    ~RecordingPanel()override{stopTimer();}
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff1b2532));g.setColour(juce::Colour(0xffc3d0df));g.setFont(13.0f);
        g.drawText("Entradas ativas do dispositivo (configure em Audio)",16,58,600,22,juce::Justification::centredLeft);g.drawText("Punch In / Out (segundos)",16,202,245,22,juce::Justification::centredLeft);g.drawText("Pre / Post roll (segundos)",330,202,290,22,juce::Justification::centredLeft);g.drawText("Loop Start / End (segundos)",16,275,390,22,juce::Justification::centredLeft);
        if(valid())g.drawText("Input peak: "+juce::String(juce::Decibels::gainToDecibels(engine.track(track).inputPeak.load(),-90.0f),1)+" dBFS",330,275,290,22,juce::Justification::centredLeft);}
    void resized()override{tracks.setBounds(16,16,608,30);left.setBounds(16,85,292,28);right.setBounds(330,85,294,28);stereo.setBounds(16,123,110,28);arm.setBounds(140,123,105,28);monitor.setBounds(260,123,165,28);gain.setBounds(16,162,608,30);
        punch.setBounds(16,236,100,28);punchIn.setBounds(124,236,85,28);punchOut.setBounds(218,236,90,28);preRoll.setBounds(330,236,110,28);postRoll.setBounds(452,236,110,28);
        loop.setBounds(16,305,120,28);loopStart.setBounds(145,305,105,28);loopEnd.setBounds(265,305,110,28);start.setBounds(16,355,190,34);stop.setBounds(220,355,155,34);recover.setBounds(390,355,234,34);takes.setBounds(16,413,450,30);activate.setBounds(480,413,144,30);status.setBounds(16,458,608,65);}
    bool validateBindings(){if(!valid())return false;const bool originalArm=engine.track(track).recordArm;arm.setToggleState(!originalArm,juce::dontSendNotification);arm.onClick();const bool armed=engine.track(track).recordArm==!originalArm;arm.setToggleState(originalArm,juce::dontSendNotification);arm.onClick();
        gain.setValue(-6,juce::sendNotificationSync);const bool gainChanged=std::abs(engine.track(track).recordGain.load()-juce::Decibels::decibelsToGain(-6.0f))<1e-6;gain.setValue(0,juce::sendNotificationSync);
        stereo.setToggleState(true,juce::dontSendNotification);stereo.onClick();const bool routing=engine.track(track).recordStereo;stereo.setToggleState(false,juce::dontSendNotification);stereo.onClick();
        activate.onClick();return armed&&gainChanged&&routing&&!engine.track(track).monitorInput;}
private:
    AudioRuntime& runtime;AudioEngine& engine;juce::File folder;std::function<void()> changed;int track=-1;
    juce::ComboBox tracks,left,right,takes;juce::ToggleButton stereo{"Stereo"},arm{"Record Arm"},monitor{"Input Monitoring"},punch{"Punch"},loop{"Loop Record"};juce::Slider gain;juce::TextEditor punchIn,punchOut,preRoll,postRoll,loopStart,loopEnd;
    juce::TextButton start{"Record"},stop{"Stop"},activate{"Ativar take"},recover{"Recuperar captura"};juce::Label status;std::unique_ptr<juce::FileChooser> chooser;
    bool valid()const{return track>=0&&track<engine.trackCount()&&!engine.track(track).midiTrack&&!engine.track(track).auxiliary;}
    void configure(){if(!valid())return;if(runtime.audioRecorder->isActive()){juce::String error;runtime.audioRecorder->stop(error);}AudioEngine::MutationGuard guard(engine);engine.track(track).recordInputLeft=std::max(0,left.getSelectedId()-1);engine.track(track).recordInputRight=std::max(0,right.getSelectedId()-1);engine.track(track).recordStereo=stereo.getToggleState();changed();}
    void refresh(){if(!valid())return;auto& t=engine.track(track);left.setSelectedId(t.recordInputLeft+1,juce::dontSendNotification);right.setSelectedId(t.recordInputRight+1,juce::dontSendNotification);stereo.setToggleState(t.recordStereo,juce::dontSendNotification);arm.setToggleState(t.recordArm,juce::dontSendNotification);monitor.setToggleState(t.monitorInput,juce::dontSendNotification);gain.setValue(juce::Decibels::gainToDecibels(t.recordGain.load()),juce::dontSendNotification);
        const int selection=takes.getSelectedId();takes.clear(juce::dontSendNotification);for(int i=0;i<int(t.takes.size());++i)takes.addItem(t.takes[i].file.getFileName()+" / "+juce::String(t.takes[i].length,2)+"s",i+1);if(takes.getNumItems())takes.setSelectedId(std::clamp(selection,1,takes.getNumItems()),juce::dontSendNotification);}
    void timerCallback()override{if(!valid())return;repaint();if(runtime.audioRecorder->isActive())status.setText(runtime.audioRecorder->status(),juce::dontSendNotification);else refresh();}
};
class RecordingWindow final:public juce::DocumentWindow {
public:explicit RecordingWindow(std::unique_ptr<RecordingPanel> panel):DocumentWindow("NOVA Recording",juce::Colour(0xff1b2532),closeButton){setUsingNativeTitleBar(true);setContentOwned(panel.release(),true);centreWithSize(640,550);setVisible(true);}
    void closeButtonPressed()override{setVisible(false);}
};
}
bool validateRecordingEditorBindings(juce::DocumentWindow& window){if(auto* panel=dynamic_cast<RecordingPanel*>(window.getContentComponent()))return panel->validateBindings();return false;}
std::unique_ptr<juce::DocumentWindow> createRecordingEditor(AudioRuntime& runtime,int track,const juce::File& folder,std::function<void()> changed){return std::make_unique<RecordingWindow>(std::make_unique<RecordingPanel>(runtime,track,folder,std::move(changed)));}
}
