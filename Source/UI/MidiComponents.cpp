#include "MidiComponents.h"
#include "Midi/MidiEditModel.h"
#include "Midi/MidiRuntime.h"
#include <cmath>
namespace nova {
namespace {
const juce::Colour background{0xff121923},panel{0xff202b39},accent{0xff59d9c3},line{0xff344253};
class MidiCanvas final : public juce::Component {
public:
    explicit MidiCanvas(MidiEditModel& value):model(value){setWantsKeyboardFocus(true);}
    double pixelsPerBeat=100,rowHeight=16;bool draw=true;
    int velocity=100,channel=1;
    std::function<void()> changed;
    std::function<bool(const juce::KeyPress&)> key;
    void updateSize(){double beats=16;if(auto* clip=model.clip()){beats=std::max(beats,double(clip->offset+clip->length)/midiPpq+2);for(const auto& note:clip->notes)beats=std::max(beats,double(note.tick+note.length)/midiPpq+2);}setSize(std::min(1000000,int(beats*pixelsPerBeat)),int(128*rowHeight));repaint();}
    juce::Rectangle<float> noteBounds(const MidiNote& note)const{return {float(note.tick/double(midiPpq)*pixelsPerBeat),float((127-note.pitch)*rowHeight+1),float(std::max(5.0,note.length/double(midiPpq)*pixelsPerBeat)),float(rowHeight-2)};}
    void paint(juce::Graphics& g)override{
        g.fillAll(background);const auto visible=g.getClipBounds();for(int pitch=0;pitch<128;++pitch){const int semitone=pitch%12;const bool black=semitone==1||semitone==3||semitone==6||semitone==8||semitone==10;
            const int y=int((127-pitch)*rowHeight);if(y+rowHeight<visible.getY()||y>visible.getBottom())continue;g.setColour(black?juce::Colour(0xff161e29):juce::Colour(0xff1b2532));g.fillRect(visible.getX(),y,visible.getWidth(),int(rowHeight));g.setColour(line);g.drawHorizontalLine(y,float(visible.getX()),float(visible.getRight()));}
        const double step=std::max(0.0625,model.grid/double(midiPpq));const int first=int(visible.getX()/pixelsPerBeat/step);
        for(int beat=first;beat*step*pixelsPerBeat<visible.getRight()+1;++beat){const int x=int(beat*step*pixelsPerBeat);g.setColour(beat%4==0?juce::Colour(0xff4b6077):line);g.drawVerticalLine(x,float(visible.getY()),float(visible.getBottom()));}
        if(auto* clip=model.clip())for(const auto& note:clip->notes){auto bounds=noteBounds(note);if(!bounds.intersects(visible.toFloat()))continue;
            g.setColour(model.selected.contains(note.id)?accent:juce::Colour(0xff758dd5));g.fillRoundedRectangle(bounds,2);g.setColour(juce::Colours::black.withAlpha(0.4f));g.drawRoundedRectangle(bounds,2,1);
            g.setColour(juce::Colours::white.withAlpha(0.7f));g.fillRect(bounds.withHeight(2).withWidth(bounds.getWidth()*note.velocity/127.0f));}
        if(dragging&&(!lasso)&&model.clip()){g.setColour(juce::Colours::white.withAlpha(0.4f));for(const auto& note:model.clip()->notes)if(model.selected.contains(note.id)){auto preview=note;preview.tick=std::max<int64_t>(0,preview.tick+deltaTick);preview.pitch=std::clamp(preview.pitch+deltaPitch,0,127);if(resize){preview.tick=note.tick;preview.pitch=note.pitch;preview.length=std::max<int64_t>(1,note.length+deltaTick);}g.drawRect(noteBounds(preview),1);}}
        if(dragging&&lasso){g.setColour(accent.withAlpha(0.15f));g.fillRect(selection);g.setColour(accent);g.drawRect(selection,1);}
    }
    void mouseDown(const juce::MouseEvent& event)override{
        grabKeyboardFocus();origin=event.position;deltaTick=0;deltaPitch=0;dragging=false;lasso=false;resize=false;model.cursor=model.quantize(int64_t(event.x/pixelsPerBeat*midiPpq));
        auto* clip=model.clip();if(!clip)return;
        for(auto i=clip->notes.rbegin();i!=clip->notes.rend();++i)if(noteBounds(*i).contains(event.position)){
            if(event.mods.isShiftDown())model.select(i->id,true);else if(!model.selected.contains(i->id))model.select(i->id);
            resize=event.x>noteBounds(*i).getRight()-6;dragging=true;if(changed)changed();repaint();return;
        }
        if(draw&&!event.mods.isShiftDown()){model.addNote(model.cursor,std::clamp(127-int(event.y/rowHeight),0,127),model.grid,velocity,channel);if(changed)changed();updateSize();}
        else {if(!event.mods.isShiftDown())model.selected.clear();dragging=lasso=true;selection={event.position,{0,0}};}
    }
    void mouseDrag(const juce::MouseEvent& event)override{
        if(!dragging)return;if(lasso)selection=juce::Rectangle<float>(origin,event.position).getUnion(juce::Rectangle<float>(event.position,origin));
        else {const auto raw=int64_t(std::llround((event.position.x-origin.x)/pixelsPerBeat*midiPpq));deltaTick=model.snap?std::llround(double(raw)/model.grid)*model.grid:raw;deltaPitch=resize?0:int(std::llround((origin.y-event.position.y)/rowHeight));}repaint();
    }
    void mouseUp(const juce::MouseEvent&)override{
        if(dragging){if(lasso){if(auto* clip=model.clip())for(const auto& note:clip->notes)if(noteBounds(note).intersects(selection))model.selected.insert(note.id);}
            else if(resize)model.resizeSelected(deltaTick);else model.moveSelected(deltaTick,deltaPitch);dragging=false;if(changed)changed();updateSize();}
    }
    bool keyPressed(const juce::KeyPress& press)override{return key?key(press):false;}
private:
    MidiEditModel& model;juce::Point<float> origin;bool dragging=false,lasso=false,resize=false;int64_t deltaTick=0;int deltaPitch=0;juce::Rectangle<float> selection;
};
class PianoKeys final : public juce::Component {
public:
    AudioEngine& engine;int track;double rowHeight=16;int scrollY=0,channel=1,heldChannel=1;int held=-1;
    PianoKeys(AudioEngine& owner,int index):engine(owner),track(index){}
    ~PianoKeys()override{release();}
    void release(){if(held>=0){engine.postMidi(track,juce::MidiMessage::noteOff(heldChannel,held));held=-1;}}
    void paint(juce::Graphics& g)override{g.fillAll(panel);for(int pitch=0;pitch<128;++pitch){const float y=float((127-pitch)*rowHeight-scrollY);if(y+rowHeight<0||y>getHeight())continue;
        const int key=pitch%12;const bool black=key==1||key==3||key==6||key==8||key==10;g.setColour(pitch==held?accent:black?juce::Colour(0xff17202b):juce::Colour(0xffd4dce7));g.fillRect(0.0f,y,float(getWidth()-2),float(rowHeight-1));
        if(key==0){g.setColour(black?juce::Colours::white:juce::Colours::black);g.setFont(11.0f);g.drawText("C"+juce::String(pitch/12-1),4,int(y),getWidth()-8,int(rowHeight),juce::Justification::centredLeft);}}}
    void mouseDown(const juce::MouseEvent& event)override{release();held=std::clamp(127-int((event.y+scrollY)/rowHeight),0,127);heldChannel=channel;engine.postMidi(track,juce::MidiMessage::noteOn(channel,held,uint8_t(100)));repaint();}
    void mouseUp(const juce::MouseEvent&)override{release();repaint();}
};
class MidiViewport final : public juce::Viewport {
public:std::function<void(juce::Rectangle<int>)> changed;
private:void visibleAreaChanged(const juce::Rectangle<int>& area)override{if(changed)changed(area);}
};
class MidiPanel final : public juce::Component,private juce::Timer {
public:
    MidiPanel(AudioRuntime& owner,int index,PluginCatalog& list,std::function<void()> notify,juce::File recovery,uint64_t selectedClip)
        :runtime(owner),engine(owner.engine),track(index),catalog(list),changed(std::move(notify)),folder(std::move(recovery)),model(engine,index),canvas(model),keys(engine,index){
        for(int i=0;i<int(engine.track(index).midiClips.size());++i)if(engine.track(index).midiClips[i].id==selectedClip)model.clipIndex=i;
        for(auto* child:std::initializer_list<juce::Component*>{&instrument,&input,&presets,&clips,&grid,&channel,&velocity,&horizontal,&vertical,&viewport,&keys,&draw,&snap,&copy,&paste,&erase,&quantize,&undo,&redo,&addClip,&duplicate,&open,&savePreset,&loadPreset,&arm,&record,&play,&stop,&import,&exportMidi,&recover,&clipLoop,&click,&clipStart,&clipLength,&clipOffset,&clipApply,&footer})addAndMakeVisible(child);
        viewport.setViewedComponent(&canvas,false);viewport.changed=[this](auto area){keys.scrollY=area.getY();keys.repaint();repaint();};
        canvas.changed=[this]{changed();refresh();};canvas.key=[this](const auto& key){return keyPressed(key);};
        for(auto* button:{&draw,&snap,&arm})button->setClickingTogglesState(true);draw.setToggleState(true,juce::dontSendNotification);snap.setToggleState(true,juce::dontSendNotification);
        draw.onClick=[this]{canvas.draw=draw.getToggleState();};snap.onClick=[this]{model.snap=snap.getToggleState();};
        grid.addItemList({"1/4","1/8","1/16","1/32"},1);grid.setSelectedId(3);grid.onChange=[this]{model.grid=midiPpq>>(grid.getSelectedId()-1);canvas.repaint();};
        for(int i=1;i<=16;++i)channel.addItem("Canal "+juce::String(i),i);channel.setSelectedId(1);channel.onChange=[this]{canvas.channel=keys.channel=channel.getSelectedId();if(model.setChannel(channel.getSelectedId()))changed();};
        velocity.setRange(1,127,1);velocity.setValue(100);velocity.setTextValueSuffix(" velocity");velocity.setTextBoxStyle(juce::Slider::TextBoxRight,false,105,22);
        velocity.onValueChange=[this]{canvas.velocity=int(velocity.getValue());if(model.setVelocity(canvas.velocity))changed();canvas.repaint();};
        horizontal.setRange(35,200,5);horizontal.setValue(100);horizontal.setTextBoxStyle(juce::Slider::TextBoxRight,false,62,22);horizontal.onValueChange=[this]{canvas.pixelsPerBeat=horizontal.getValue();canvas.updateSize();};
        vertical.setRange(8,32,1);vertical.setValue(16);vertical.setTextBoxStyle(juce::Slider::TextBoxRight,false,50,22);vertical.onValueChange=[this]{canvas.rowHeight=keys.rowHeight=vertical.getValue();canvas.updateSize();keys.repaint();};
        copy.onClick=[this]{model.copy();refresh();};paste.onClick=[this]{action(model.paste(model.cursor));};erase.onClick=[this]{action(model.eraseSelected());};quantize.onClick=[this]{action(model.quantizeSelected());};undo.onClick=[this]{action(model.undo());};redo.onClick=[this]{action(model.redo());};
        addClip.onClick=[this]{action(model.addClip());};duplicate.onClick=[this]{action(model.duplicateClip());};clips.onChange=[this]{model.clipIndex=clips.getSelectedId()-1;model.selected.clear();refresh();};
        arm.setToggleState(engine.track(track).midiArm,juce::dontSendNotification);arm.onClick=[this]{engine.track(track).midiArm=arm.getToggleState();changed();};
        clipLoop.setClickingTogglesState(true);click.setClickingTogglesState(true);
        clipLoop.onClick=[this]{if(auto* clip=model.clip()){if(clipLoop.getToggleState()){const double begin=ticksToSeconds(clip->start,engine.bpm.load()),end=ticksToSeconds(clip->start+clip->length,engine.bpm.load());engine.setLoop(begin,end,true);engine.seek(begin);}else engine.loopEnabled=false;changed();}};
        click.onClick=[this]{engine.metronomeEnabled=click.getToggleState();changed();};
        record.onClick=[this]{juce::String error;if(runtime.midiRecorder->isActive()){if(!runtime.midiRecorder->stop(error))footer.setText(error,juce::dontSendNotification);}
            else {if(runtime.midiRecorder->start(folder,error))model.checkpointExternal();else footer.setText(error,juce::dontSendNotification);}changed();refresh();};
        play.onClick=[this]{engine.play();};stop.onClick=[this]{juce::String error;runtime.midiRecorder->stop(error);engine.stop();refresh();};
        input.addItem("Entrada MIDI: nenhuma",1);devices=juce::MidiInput::getAvailableDevices();for(int i=0;i<devices.size();++i)input.addItem(devices[i].name,i+2);input.setSelectedId(1);
        for(int i=0;i<devices.size();++i)if(devices[i].identifier==engine.track(track).midiInput)input.setSelectedId(i+2,juce::dontSendNotification);
        input.onChange=[this]{juce::String error;const int selected=input.getSelectedId()-2;const auto id=selected>=0&&selected<devices.size()?devices[selected].identifier:juce::String{};
            if(!runtime.ensureMidiInput(id,error)){footer.setText(error,juce::dontSendNotification);return;}engine.setMidiInput(track,id);changed();};
        instrument.onChange=[this]{const int selected=instrument.getSelectedId()-2;AudioEngine::MutationGuard guard(engine);auto& current=engine.track(track).instrument;
            if(selected<0){current.reset();engine.prepareProcessors();changed();refresh();return;}if(selected>=types.size())return;
            juce::String error;auto replacement=MixProcessor::loadPlugin(types[selected],engine.sampleRate(),error);if(!replacement||!replacement->plugin){footer.setText(replacement?replacement->failure:error,juce::dontSendNotification);return;}
            current=std::move(replacement);engine.prepareProcessors();engine.seek(engine.position());changed();refresh();};
        open.onClick=[this]{AudioEngine::MutationGuard guard(engine);if(auto& p=engine.track(track).instrument)p->openEditor();};
        presets.onChange=[this]{AudioEngine::MutationGuard guard(engine);if(auto& p=engine.track(track).instrument)if(p->plugin&&presets.getSelectedId()>0){p->plugin->setCurrentProgram(presets.getSelectedId()-1);changed();}};
        import.onClick=[this]{chooseFile(false,false);};exportMidi.onClick=[this]{chooseFile(true,false);};savePreset.onClick=[this]{chooseFile(true,true);};loadPreset.onClick=[this]{chooseFile(false,true);};
        recover.onClick=[this]{juce::String error;footer.setText(runtime.midiRecorder->saveRecovery(folder,error)?runtime.midiRecorder->status():error,juce::dontSendNotification);};
        clipApply.onClick=[this]{const auto a=clipStart.getText().getDoubleValue(),b=clipLength.getText().getDoubleValue(),c=clipOffset.getText().getDoubleValue();if(std::isfinite(a)&&std::isfinite(b)&&std::isfinite(c))action(model.setClipTiming(std::llround(a*midiPpq),std::llround(b*midiPpq),std::llround(c*midiPpq)));};
        for(auto* text:{&clipStart,&clipLength,&clipOffset})text->setInputRestrictions(16,"0123456789.");
        setSize(1120,760);refresh();canvas.updateSize();viewport.setViewPosition(0,int((127-75)*canvas.rowHeight));startTimerHz(10);
    }
    ~MidiPanel()override{stopTimer();keys.release();viewport.setViewedComponent(nullptr);}
    void releaseKeyboard(){keys.release();}
    void resized()override{
        const int width=getWidth();instrument.setBounds(12,10,250,26);open.setBounds(268,10,74,26);presets.setBounds(350,10,160,26);savePreset.setBounds(518,10,95,26);loadPreset.setBounds(620,10,95,26);input.setBounds(725,10,std::max(150,width-738),26);
        clips.setBounds(12,43,200,25);addClip.setBounds(220,43,75,25);duplicate.setBounds(300,43,90,25);clipStart.setBounds(400,43,65,25);clipLength.setBounds(470,43,65,25);clipOffset.setBounds(540,43,65,25);clipApply.setBounds(610,43,90,25);
        arm.setBounds(710,43,65,25);record.setBounds(782,43,90,25);play.setBounds(880,43,65,25);stop.setBounds(952,43,65,25);
        draw.setBounds(12,76,80,25);snap.setBounds(98,76,65,25);grid.setBounds(170,76,72,25);channel.setBounds(248,76,104,25);velocity.setBounds(360,76,180,25);horizontal.setBounds(550,76,160,25);vertical.setBounds(720,76,140,25);import.setBounds(870,76,95,25);exportMidi.setBounds(970,76,105,25);
        copy.setBounds(12,108,65,25);paste.setBounds(84,108,65,25);erase.setBounds(155,108,65,25);quantize.setBounds(226,108,90,25);undo.setBounds(322,108,65,25);redo.setBounds(394,108,65,25);recover.setBounds(470,108,135,25);clipLoop.setBounds(615,108,85,25);click.setBounds(710,108,100,25);
        keys.setBounds(0,159,64,getHeight()-192);viewport.setBounds(66,159,width-66,getHeight()-192);footer.setBounds(12,getHeight()-28,width-24,24);
        canvas.updateSize();
    }
    void paint(juce::Graphics& g)override{g.fillAll(panel);g.setColour(juce::Colour(0xffbac9d9));g.setFont(11.0f);g.drawText("inicio / duracao / offset (beats)",400,132,305,18,juce::Justification::centredLeft);
        const double offset=viewport.getViewPositionX(),beatPixels=canvas.pixelsPerBeat*4.0/engine.beatUnit;for(int beat=int(offset/beatPixels);beat*beatPixels<offset+getWidth();++beat){const int x=int(66+beat*beatPixels-offset);g.setColour(line);g.drawVerticalLine(x,140,158);g.setColour(juce::Colours::white);g.drawText(juce::String(beat/engine.beatsPerBar+1)+"|"+juce::String(beat%engine.beatsPerBar+1),x+3,140,70,18,juce::Justification::centredLeft);}}
    bool keyPressed(const juce::KeyPress& key)override{
        if(key==juce::KeyPress::deleteKey){erase.onClick();return true;}if(key.getModifiers().isCommandDown()){
            if(key.getKeyCode()=='A'){model.selectAll();refresh();return true;}if(key.getKeyCode()=='C'){copy.onClick();return true;}if(key.getKeyCode()=='V'){paste.onClick();return true;}
            if(key.getKeyCode()=='X'){model.copy();erase.onClick();return true;}if(key.getKeyCode()=='Z'){if(key.getModifiers().isShiftDown())redo.onClick();else undo.onClick();return true;}if(key.getKeyCode()=='Y'){redo.onClick();return true;}}
        if(key==juce::KeyPress::spaceKey){if(engine.isPlaying())engine.pause();else engine.play();return true;}return false;
    }
    bool validateBindings(){
        if(!model.clip())return false;const auto count=model.clip()->notes.size();
        auto event=[this](float x,float y){const auto now=juce::Time::getCurrentTime();return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),{x,y},juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&canvas,&canvas,now,{x,y},now,1,true);};
        const float y60=float((127-60)*canvas.rowHeight+canvas.rowHeight/2);canvas.mouseDown(event(4,y60));const auto id=*model.selected.begin();
        canvas.mouseDown(event(10,y60));canvas.mouseDrag(event(35,y60-float(2*canvas.rowHeight)));canvas.mouseUp(event(35,y60-float(2*canvas.rowHeight)));
        const float y62=y60-float(2*canvas.rowHeight);canvas.mouseDown(event(48,y62));canvas.mouseDrag(event(148,y62));canvas.mouseUp(event(148,y62));
        velocity.setValue(80,juce::sendNotificationSync);copy.onClick();model.cursor=1920;paste.onClick();model.moveSelected(13,0);
        canvas.keyPressed(juce::KeyPress('A',juce::ModifierKeys::ctrlModifier,0));quantize.onClick();
        const bool edits=model.clip()->notes.size()>=count+2&&std::any_of(model.clip()->notes.begin(),model.clip()->notes.end(),[&](const auto& note){return note.id==id&&note.pitch==62&&note.velocity==80&&note.length==1200;});
        erase.onClick();const bool removed=model.clip()->notes.empty();undo.onClick();const bool restored=model.clip()->notes.size()>=2;redo.onClick();undo.onClick();refresh();return edits&&removed&&restored;
    }
private:
    AudioRuntime& runtime;AudioEngine& engine;int track;PluginCatalog& catalog;std::function<void()> changed;juce::File folder;MidiEditModel model;MidiCanvas canvas;PianoKeys keys;MidiViewport viewport;
    juce::ComboBox instrument,input,presets,clips,grid,channel;juce::Slider velocity,horizontal,vertical;juce::Label footer;
    juce::TextEditor clipStart,clipLength,clipOffset;juce::TextButton clipApply{"Aplicar clip"};
    juce::TextButton clipLoop{"Loop clip"},click{"Metronomo"};
    juce::TextButton draw{"Desenhar"},snap{"Snap"},copy{"Copy"},paste{"Paste"},erase{"Delete"},quantize{"Quantizar"},undo{"Undo"},redo{"Redo"},addClip{"+ Clip"},duplicate{"Duplicar"},open{"Interface"},savePreset{"Salvar preset"},loadPreset{"Abrir preset"},arm{"Arm"},record{"Record MIDI"},play{"Play"},stop{"Stop"},import{"Importar .mid"},exportMidi{"Exportar .mid"},recover{"Recuperar MIDI"};
    juce::Array<juce::MidiDeviceInfo> devices;juce::Array<juce::PluginDescription> types;std::unique_ptr<juce::FileChooser> chooser;int knownCount=-1,lastClipCount=-1;
    void action(bool success){if(success)changed();else if(model.error().isNotEmpty())footer.setText(model.error(),juce::dontSendNotification);refresh();}
    void refresh(){
        if(!model.valid())return;auto& t=engine.track(track);const auto all=catalog.known.getTypes();if(all.size()!=knownCount){knownCount=all.size();types.clear();instrument.clear(juce::dontSendNotification);instrument.addItem("Instrumento: nenhum",1);
            for(const auto& description:all)if(description.isInstrument){types.add(description);instrument.addItem(description.name,types.size()+1);}}
        int selectedInstrument=1;if(t.instrument)for(int i=0;i<types.size();++i)if(types[i].fileOrIdentifier==t.instrument->description.fileOrIdentifier)selectedInstrument=i+2;instrument.setSelectedId(selectedInstrument,juce::dontSendNotification);
        if(t.midiClips.size()!=lastClipCount){lastClipCount=int(t.midiClips.size());clips.clear(juce::dontSendNotification);for(int i=0;i<lastClipCount;++i)clips.addItem(t.midiClips[i].name+" "+juce::String(i+1),i+1);}clips.setSelectedId(model.clipIndex+1,juce::dontSendNotification);
        presets.clear(juce::dontSendNotification);if(t.instrument&&t.instrument->plugin){AudioEngine::MutationGuard guard(engine);for(int i=0;i<t.instrument->plugin->getNumPrograms();++i)presets.addItem(t.instrument->plugin->getProgramName(i),i+1);presets.setSelectedId(t.instrument->plugin->getCurrentProgram()+1,juce::dontSendNotification);}
        open.setEnabled(t.instrument&&t.instrument->plugin);savePreset.setEnabled(t.instrument!=nullptr);undo.setEnabled(model.canUndo());redo.setEnabled(model.canRedo());paste.setEnabled(model.canPaste());arm.setToggleState(t.midiArm,juce::dontSendNotification);record.setButtonText(runtime.midiRecorder->isActive()?"Parar MIDI":"Record MIDI");clipLoop.setToggleState(engine.loopEnabled,juce::dontSendNotification);click.setToggleState(engine.metronomeEnabled,juce::dontSendNotification);
        if(auto* clip=model.clip()){if(!clipStart.hasKeyboardFocus(true))clipStart.setText(juce::String(clip->start/double(midiPpq),3),false);if(!clipLength.hasKeyboardFocus(true))clipLength.setText(juce::String(clip->length/double(midiPpq),3),false);if(!clipOffset.hasKeyboardFocus(true))clipOffset.setText(juce::String(clip->offset/double(midiPpq),3),false);
            if(model.selected.size()==1)for(const auto& note:clip->notes)if(model.selected.contains(note.id)){velocity.setValue(note.velocity,juce::dontSendNotification);channel.setSelectedId(note.channel,juce::dontSendNotification);canvas.velocity=note.velocity;canvas.channel=keys.channel=note.channel;}}
        canvas.updateSize();keys.repaint();
    }
    void chooseFile(bool saving,bool preset){
        chooser=std::make_unique<juce::FileChooser>(preset?"Preset do instrumento":"Standard MIDI File",juce::File{},preset?"*.novapreset":"*.mid;*.midi");
        const int flags=(saving?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles;
        chooser->launchAsync(flags,[safe=juce::Component::SafePointer<MidiPanel>(this),saving,preset](const auto& chosen){if(!safe||chosen.getResult()==juce::File{})return;auto& self=*safe;juce::String error;
            if(preset){AudioEngine::MutationGuard guard(self.engine);if(saving){if(auto& p=self.engine.track(self.track).instrument)chosen.getResult().withFileExtension("novapreset").replaceWithText(juce::JSON::toString(p->state()));}
                else{const auto file=chosen.getResult();if(file.getSize()>64LL*1024*1024){error="Preset excede 64 MiB.";}else{auto p=MixProcessor::restore(juce::JSON::parse(file.loadFileAsString()),self.engine.sampleRate(),error);if(p&&p->isInstrument()){self.engine.track(self.track).instrument=std::move(p);self.engine.prepareProcessors();self.changed();}else error="Preset de instrumento invalido.";}}}
            else if(saving){if(auto* clip=self.model.clip())exportSmf(chosen.getResult().withFileExtension("mid"),{*clip},self.engine.bpm.load(),self.engine.beatsPerBar,self.engine.beatUnit,error);}
            else self.action(self.model.importFile(chosen.getResult()));
            if(error.isNotEmpty())self.footer.setText(error,juce::dontSendNotification);self.refresh();});
    }
    void timerCallback()override{if(!model.valid()){stopTimer();return;}refresh();if(runtime.midiRecorder->isActive())footer.setText(runtime.midiRecorder->status(),juce::dontSendNotification);}
};
class MidiWindow final : public juce::DocumentWindow {
public:
    explicit MidiWindow(std::unique_ptr<MidiPanel> content):DocumentWindow("NOVA Piano Roll",panel,closeButton){setUsingNativeTitleBar(true);setContentOwned(content.release(),true);setResizable(true,false);centreWithSize(1120,760);setVisible(true);}
    void closeButtonPressed()override{if(auto* content=dynamic_cast<MidiPanel*>(getContentComponent()))content->releaseKeyboard();setVisible(false);}
};
}
std::unique_ptr<juce::DocumentWindow> createMidiEditor(AudioRuntime& runtime,int track,PluginCatalog& catalog,std::function<void()> changed,const juce::File& recovery,uint64_t selectedClip){return std::make_unique<MidiWindow>(std::make_unique<MidiPanel>(runtime,track,catalog,std::move(changed),recovery,selectedClip));}
bool validateMidiEditorBindings(juce::DocumentWindow& window){if(auto* content=dynamic_cast<MidiPanel*>(window.getContentComponent()))return content->validateBindings();return false;}
}
