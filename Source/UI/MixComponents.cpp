#include "MixComponents.h"
#include <cmath>
namespace nova {
namespace {
const juce::Colour panel{0xff1c2532},accent{0xff59d9c3};
struct ToolWindow final : juce::DocumentWindow {
    ToolWindow(juce::Component* content,const juce::String& title):DocumentWindow(title,panel,closeButton){
        setUsingNativeTitleBar(true);setContentOwned(content,true);setResizable(true,false);centreWithSize(content->getWidth(),content->getHeight());setVisible(true);
    }
    void closeButtonPressed()override{setVisible(false);}
};
class PluginManagerPanel final : public juce::Component,private juce::Timer {
public:
    explicit PluginManagerPanel(PluginCatalog& c):catalog(c){for(auto* child:std::initializer_list<juce::Component*>{&list,&details,&remove,&clear})addAndMakeVisible(child);
        details.setMultiLine(true);details.setReadOnly(true);list.onChange=[this]{updateDetails();};
        remove.onClick=[this]{const int i=list.getSelectedId()-1;if(i>=0&&i<types.size()){catalog.remove(types[i]);refreshList();}};
        clear.onClick=[this]{catalog.clearBlacklist();updateDetails();};refreshList();setSize(600,390);startTimer(500);
    }
    ~PluginManagerPanel()override{stopTimer();}
    void resized()override{list.setBounds(15,15,getWidth()-30,30);details.setBounds(15,55,getWidth()-30,getHeight()-115);remove.setBounds(15,getHeight()-45,200,30);clear.setBounds(225,getHeight()-45,220,30);}
private:
    PluginCatalog& catalog;juce::Array<juce::PluginDescription> types;juce::ComboBox list;juce::TextEditor details;juce::TextButton remove{"Remover da lista"},clear{"Limpar registro de falhas"};
    void refreshList(){types=catalog.known.getTypes();const int selected=list.getSelectedId();list.clear();for(int i=0;i<types.size();++i)list.addItem(types[i].name,i+1);if(!types.isEmpty())list.setSelectedId(std::clamp(selected,1,types.size()),juce::dontSendNotification);updateDetails();}
    void updateDetails(){juce::String text=catalog.status()+"\nPastas de busca:\n"+catalog.configuredLocations().toString()+"\n\n";const int i=list.getSelectedId()-1;if(i>=0&&i<types.size()){const auto& p=types[i];text+=p.name+"\n"+p.manufacturerName+" "+p.version+"\n"+p.fileOrIdentifier+"\n";}
        text+="\nModulos com falha:\n"+catalog.known.getBlacklistedFiles().joinIntoString("\n");details.setText(text);remove.setEnabled(i>=0&&!catalog.scanning());clear.setEnabled(!catalog.scanning());}
    void timerCallback()override{if(types.size()!=catalog.known.getNumTypes())refreshList();else updateDetails();}
};
class EffectPanel final : public juce::Component,private juce::Timer {
public:
    EffectPanel(MixProcessor& p,std::function<void()> c,double rate):processor(p),changed(std::move(c)),sampleRate(rate){
        for(int i=0;i<p.parameterCount();++i){auto slider=std::make_unique<juce::Slider>();const auto spec=p.spec(i);slider->setRange(spec.minimum,spec.maximum,0);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,75,22);slider->setValue(p.parameter(i));
            slider->onValueChange=[this,i,s=slider.get()]{processor.setParameter(i,static_cast<float>(s->getValue()));changed();repaint();};
            auto label=std::make_unique<juce::Label>();label->setText(spec.name,juce::dontSendNotification);addAndMakeVisible(*label);addAndMakeVisible(*slider);labels.push_back(std::move(label));sliders.push_back(std::move(slider));}
        bypass.setClickingTogglesState(true);bypass.setToggleState(p.bypass,juce::dontSendNotification);bypass.onClick=[this]{processor.bypass=bypass.getToggleState();changed();};addAndMakeVisible(bypass);
        setSize(p.kind==EffectKind::equalizer?850:480,p.kind==EffectKind::equalizer?500:380);startTimerHz(20);
    }
    ~EffectPanel()override{stopTimer();}
    void resized()override{bypass.setBounds(15,10,100,26);const int cols=processor.kind==EffectKind::equalizer?3:1,width=(getWidth()-30)/cols;
        for(int i=0;i<int(sliders.size());++i){const int x=15+(i%cols)*width,y=48+(i/cols)*42;labels[i]->setBounds(x,y,width-8,17);sliders[i]->setBounds(x,y+17,width-8,23);}}
    void paint(juce::Graphics& g)override{g.fillAll(panel);g.setColour(accent);
        if(processor.kind==EffectKind::equalizer){auto r=getLocalBounds().toFloat().withTrimmedTop(355).reduced(15);g.setColour(juce::Colour(0xff0e141d));g.fillRect(r);
            juce::Path curve;for(int x=0;x<int(r.getWidth());++x){const double hz=20*std::pow(1000.0,x/r.getWidth());const double db=juce::Decibels::gainToDecibels(processor.magnitude(hz,sampleRate),-60.0);
                const float y=r.getCentreY()-static_cast<float>(db/36)*r.getHeight();if(x==0)curve.startNewSubPath(r.getX(),y);else curve.lineTo(r.getX()+x,y);}
            g.setColour(juce::Colour(0xff3a4558));g.drawHorizontalLine(int(r.getCentreY()),r.getX(),r.getRight());g.setColour(accent);g.strokePath(curve,juce::PathStrokeType(2));
            g.drawText("20 Hz                         1 kHz                         20 kHz",r.toNearestInt().removeFromBottom(16),juce::Justification::centred);
        }else if(processor.kind==EffectKind::compressor)g.drawText("Reducao: "+juce::String(processor.reduction.load(),1)+" dB",15,getHeight()-45,getWidth()-30,25,juce::Justification::centredLeft);
    }
    bool validateBindings(){const float old=processor.parameter(0);sliders[0]->setValue(old+1,juce::sendNotificationSync);const bool ok=processor.parameter(0)==old+1;sliders[0]->setValue(old,juce::sendNotificationSync);bypass.setToggleState(true,juce::dontSendNotification);bypass.onClick();const bool bypassOk=processor.bypass;bypass.setToggleState(false,juce::dontSendNotification);bypass.onClick();return ok&&bypassOk;}
private:
    MixProcessor& processor;std::function<void()> changed;double sampleRate;
    juce::TextButton bypass{"Bypass"};std::vector<std::unique_ptr<juce::Slider>> sliders;std::vector<std::unique_ptr<juce::Label>> labels;
    void timerCallback()override{for(int i=0;i<int(sliders.size());++i)if(!sliders[i]->isMouseButtonDown())sliders[i]->setValue(processor.parameter(i),juce::dontSendNotification);repaint();}
};
class SendPanel final : public juce::Component {
public:
    SendPanel(AudioEngine& e,int i,int slot,MixEdit a,std::function<void()> c):engine(e),index(i),sendSlot(slot),apply(std::move(a)),changed(std::move(c)){
        for(auto* component:std::initializer_list<juce::Component*>{&destination,&pre,&level,&label})addAndMakeVisible(component);
        destination.addItem("Desligado",1);for(int j=0;j<e.trackCount();++j)if(j!=i && e.track(j).auxiliary)destination.addItem(e.track(j).name,j+2);
        const auto& send=e.track(i).sends[slot];destination.setSelectedId(send.target+2,juce::dontSendNotification);
        pre.setClickingTogglesState(true);pre.setToggleState(send.preFader,juce::dontSendNotification);
        auto route=[this]{juce::String error;const int target=destination.getSelectedId()-2;const bool isPre=pre.getToggleState();
            if(!apply([&,target,isPre]{return engine.setSend(index,sendSlot,target,isPre,error);})){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Roteamento",error);destination.setSelectedId(engine.track(index).sends[sendSlot].target+2,juce::dontSendNotification);pre.setToggleState(engine.track(index).sends[sendSlot].preFader,juce::dontSendNotification);}};
        destination.onChange=route;pre.onClick=route;label.setText("Nivel do send (dB)",juce::dontSendNotification);
        level.setRange(-60,6,0.1);level.setValue(juce::Decibels::gainToDecibels(send.gain.load(),-60.0f));level.setTextBoxStyle(juce::Slider::TextBoxRight,false,75,22);
        level.onValueChange=[this]{engine.track(index).sends[sendSlot].gain=level.getValue()<=-60?0:juce::Decibels::decibelsToGain(float(level.getValue()));changed();};setSize(400,180);
    }
    void resized()override{destination.setBounds(15,15,getWidth()-30,30);pre.setBounds(15,55,160,28);label.setBounds(15,95,getWidth()-30,22);level.setBounds(15,120,getWidth()-30,30);}
    bool validateBindings(int auxiliary){destination.setSelectedId(auxiliary+2,juce::sendNotificationSync);pre.setToggleState(true,juce::dontSendNotification);pre.onClick();level.setValue(-6,juce::sendNotificationSync);
        const auto& send=engine.track(index).sends[sendSlot];return send.target==auxiliary&&send.preFader&&std::abs(send.gain.load()-juce::Decibels::decibelsToGain(-6.0f))<1e-5f;}
private:AudioEngine& engine;int index,sendSlot;MixEdit apply;std::function<void()> changed;juce::ComboBox destination;juce::TextButton pre{"Pre-fader"};juce::Slider level;juce::Label label;
};
class AutomationPanel final : public juce::Component {
public:
    AutomationPanel(AudioEngine& e,int i,MixEdit a):engine(e),index(i),apply(std::move(a)){
        for(auto* c:std::initializer_list<juce::Component*>{&target,&parameter,&points,&instructions,&commit,&mode,&result})addAndMakeVisible(c);
        target.addItem("Volume (ganho linear 0..2)",1);target.addItem("Pan (-1..1)",2);
        for(int slot=0;slot<4;++slot)if(auto& p=e.track(i).inserts[slot])target.addItem("Insert "+juce::String(slot+1)+": "+p->name(),slot+3);
        target.onChange=[this]{parameter.clear();const int dest=target.getSelectedId()-3;
            if(dest<0)parameter.addItem("Valor",1);else if(auto& p=engine.track(index).inserts[dest])for(int j=0;j<p->parameterCount();++j)parameter.addItem(p->kind==EffectKind::vst3?p->plugin->getParameters()[j]->getName(40):juce::String(p->spec(j).name),j+1);
            parameter.setSelectedId(1,juce::dontSendNotification);loadPoints();};
        parameter.onChange=[this]{loadPoints();};target.setSelectedId(1,juce::sendNotificationSync);
        mode.addItem("Off",1);mode.addItem("Read",2);mode.setSelectedId(int(e.track(i).automationMode)+1,juce::dontSendNotification);
        points.setMultiLine(true);points.setReturnKeyStartsNewLine(true);instructions.setText("Um ponto por linha: segundos : valor. Aplicar substitui a lane; vazio remove.\nParametros VST3 usam valores normalizados 0..1. Write/Touch/Latch pendentes.",juce::dontSendNotification);
        commit.onClick=[this]{AutomationLane lane;const int selected=target.getSelectedId();lane.target=selected==1?-1:selected==2?-2:selected-3;lane.parameter=parameter.getSelectedId()-1;
            if(lane.parameter<0){result.setText("Escolha um parametro disponivel.",juce::dontSendNotification);return;}
            float low=lane.target==-2?-1.0f:0,high=lane.target==-2?1.0f:2;
            if(lane.target>=0){auto& p=engine.track(index).inserts[lane.target];const auto spec=p->spec(lane.parameter);low=spec.minimum;high=spec.maximum;}
            const auto lines=juce::StringArray::fromLines(points.getText());double previous=-1;
            for(const auto& line:lines){if(line.trim().isEmpty())continue;const auto parts=juce::StringArray::fromTokens(line,":","");
                if(parts.size()!=2){result.setText("Use segundos : valor.",juce::dontSendNotification);return;}
                auto parse=[](const juce::String& text,double& value){auto utf8=text.trim().toStdString();char* end=nullptr;value=std::strtod(utf8.c_str(),&end);return end!=utf8.c_str() && *end==0 && std::isfinite(value);};
                double time=0,value=0;if(!parse(parts[0],time)||!parse(parts[1],value)||time<=previous||time<0||time>86400||value<low||value>high||lane.points.size()>=4096){result.setText("Tempos crescentes e valores dentro dos limites sao obrigatorios.",juce::dontSendNotification);return;}
                lane.points.push_back({time,float(value)});previous=time;
            }
            const auto selectedMode=static_cast<AutomationMode>(mode.getSelectedId()-1);
            const bool applied=apply([this,lane=std::move(lane),selectedMode]()mutable{auto& t=engine.track(index);auto found=std::find_if(t.automation.begin(),t.automation.end(),[&](const auto& item){return item.target==lane.target && item.parameter==lane.parameter;});
                if(found==t.automation.end()&&t.automation.size()>=64&&!lane.points.empty()){result.setText("Limite de 64 lanes por canal.",juce::dontSendNotification);return false;}
                if(found!=t.automation.end())t.automation.erase(found);if(!lane.points.empty())t.automation.push_back(std::move(lane));t.automationMode=selectedMode;return true;});
            if(applied)result.setText("Automacao aplicada.",juce::dontSendNotification);
        };setSize(620,460);
    }
    void resized()override{target.setBounds(15,15,getWidth()-30,28);parameter.setBounds(15,50,getWidth()-150,28);mode.setBounds(getWidth()-125,50,110,28);
        instructions.setBounds(15,85,getWidth()-30,65);points.setBounds(15,155,getWidth()-30,getHeight()-220);commit.setBounds(15,getHeight()-55,150,30);result.setBounds(175,getHeight()-55,getWidth()-190,30);}
    bool validateBindings(){target.setSelectedId(1,juce::sendNotificationSync);parameter.setSelectedId(1,juce::dontSendNotification);mode.setSelectedId(2,juce::dontSendNotification);points.setText("0 : 0.5\n1 : 1");commit.onClick();
        const auto& t=engine.track(index);return t.automationMode==AutomationMode::read&&std::any_of(t.automation.begin(),t.automation.end(),[](const auto& lane){return lane.target==-1&&lane.points.size()==2&&lane.points[0].value==0.5f;});}
private:AudioEngine& engine;int index;MixEdit apply;juce::ComboBox target,parameter,mode;juce::TextEditor points;juce::Label instructions,result;juce::TextButton commit{"Aplicar lane"};
    void loadPoints(){const int selected=target.getSelectedId();const int dest=selected==1?-1:selected==2?-2:selected-3;const int param=parameter.getSelectedId()-1;juce::String text;
        for(const auto& lane:engine.track(index).automation)if(lane.target==dest&&lane.parameter==param)for(const auto& p:lane.points)text+=juce::String(p.time,6)+" : "+juce::String(p.value,6)+"\n";points.setText(text);}
};
void meter(juce::Graphics& g,juce::Rectangle<float> r,float value){g.setColour(juce::Colour(0xff080d14));g.fillRect(r);
    const float amount=std::clamp((juce::Decibels::gainToDecibels(value,-60.0f)+60)/60,0.0f,1.0f);g.setColour(value>1?juce::Colours::salmon:accent);g.fillRect(r.removeFromBottom(r.getHeight()*amount));}
}
std::unique_ptr<juce::DocumentWindow> createPluginManager(PluginCatalog& catalog){return std::make_unique<ToolWindow>(new PluginManagerPanel(catalog),"Plugins VST3");}
ChannelStrip::ChannelStrip(AudioEngine& e,EditModel& ed,int i,PluginCatalog& cat,MixEdit a,std::function<void()> c):engine(e),edit(ed),index(i),catalog(cat),apply(std::move(a)),changed(std::move(c)){
    for(auto* child:std::initializer_list<juce::Component*>{&name,&input,&peakLabel,&gain,&pan,&inputGain,&mute,&solo,&clip,&colour,&automation,&output})addAndMakeVisible(child);
    for(int slot=0;slot<4;++slot){addAndMakeVisible(inserts[slot]);inserts[slot].onClick=[this,slot]{insertMenu(slot);};}
    for(int slot=0;slot<2;++slot){addAndMakeVisible(sends[slot]);sends[slot].onClick=[this,slot]{sendEditor(slot);};}
    name.setEditable(false,index>=0,false);name.onTextChange=[this]{const auto text=name.getText();apply([this,text]{edit.rename(index,text);return true;});};
    name.setFont(juce::Font(juce::FontOptions(15).withStyle("Bold")));
    gain.setRange(-60,6,0.1);gain.setSliderStyle(juce::Slider::LinearVertical);gain.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,22);gain.setTextValueSuffix(" dB");gain.setDoubleClickReturnValue(true,0);
    pan.setRange(-1,1,0.01);pan.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);pan.setTextBoxStyle(juce::Slider::TextBoxBelow,false,65,19);pan.setDoubleClickReturnValue(true,0);
    inputGain.setRange(-24,24,0.1);inputGain.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,20);inputGain.setTextValueSuffix(" dB");inputGain.setDoubleClickReturnValue(true,0);inputGain.setTooltip("Ganho de entrada");
    gain.onValueChange=[this]{const float value=gain.getValue()<=-60?0:juce::Decibels::decibelsToGain(float(gain.getValue()));if(index<0)engine.masterGain=value;else engine.track(index).gain=value;changed();};
    pan.onValueChange=[this]{if(index>=0)engine.track(index).pan=float(pan.getValue());changed();};
    inputGain.onValueChange=[this]{if(index>=0)engine.track(index).inputGain=juce::Decibels::decibelsToGain(float(inputGain.getValue()));changed();};
    mute.setClickingTogglesState(true);solo.setClickingTogglesState(true);mute.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffad5966));solo.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffa78b3d));
    mute.onClick=[this]{if(index<0)engine.masterMute=mute.getToggleState();else engine.track(index).mute=mute.getToggleState();changed();};
    solo.onClick=[this]{if(index>=0)engine.track(index).solo=solo.getToggleState();changed();};
    clip.onClick=[this]{if(index<0)engine.masterClipped=false;else engine.track(index).clipped=false;};clip.setTooltip("Pico retido; clique para limpar clipping");
    output.addItem("Master stereo",1);output.addItem("Master L",2);output.addItem("Master R",3);
    for(int j=0;j<e.trackCount();++j)if(j!=index && e.track(j).auxiliary)output.addItem(e.track(j).name,j+4);
    output.onChange=[this]{if(index<0)return;const int id=output.getSelectedId();juce::String error;
        if(!apply([&,id]{if(!engine.setRoute(index,id<=3?-1:id-4,error))return false;engine.track(index).outputMode=id<=3?id-1:0;return true;})){showError(error);refresh();}};
    colour.onClick=[this]{juce::PopupMenu menu;menu.addItem(1,"Teal");menu.addItem(2,"Azul");menu.addItem(3,"Rosa");menu.addItem(4,"Laranja");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&colour),[safe=juce::Component::SafePointer<ChannelStrip>(this)](int id){if(!safe||id<1)return;static constexpr uint32_t values[]{0xff59d9c3,0xff74a9f4,0xffeb87b3,0xffe4a362};safe->apply([safe,id]{safe->edit.recolour(safe->index,values[id-1]);return true;});});};
    automation.onClick=[this]{automationEditor();};
    if(index<0){pan.setVisible(false);inputGain.setVisible(false);solo.setVisible(false);colour.setVisible(false);automation.setVisible(false);output.setVisible(false);for(auto& send:sends)send.setVisible(false);}
    refresh();
}
ChannelStrip::~ChannelStrip(){closeEditors();}
std::array<std::unique_ptr<MixProcessor>,4>& ChannelStrip::chain(){return index<0?engine.masterInserts:engine.track(index).inserts;}
void ChannelStrip::closeEditors(){toolWindow.reset();}
void ChannelStrip::showError(const juce::String& text){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"NOVA DAW",text);}
void ChannelStrip::insertMenu(int slot){
    juce::PopupMenu menu;const auto& p=chain()[slot];if(p){menu.addItem(1,"Abrir "+p->name());menu.addItem(2,"Bypass",true,p->bypass);menu.addItem(3,"Remover");menu.addSeparator();}
    menu.addItem(10,"NOVA EQ");menu.addItem(11,"NOVA Compressor");menu.addItem(12,"NOVA Gate");menu.addItem(13,"NOVA Delay");menu.addItem(14,"NOVA Reverb");
    const auto types=catalog.known.getTypes();if(!types.isEmpty()){juce::PopupMenu vst;for(int i=0;i<types.size();++i)vst.addItem(100+i,types[i].name);menu.addSubMenu("VST3",vst);}
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&inserts[slot]),[safe=juce::Component::SafePointer<ChannelStrip>(this),slot,types](int id){
        if(!safe || !id)return;auto& p=safe->chain()[slot];
        if(id==1 && p){if(p->kind==EffectKind::vst3){if(p->plugin){AudioEngine::MutationGuard guard(safe->engine);p->openEditor();}else safe->showError(p->failure);}
            else{safe->closeEditors();safe->toolWindow=std::make_unique<ToolWindow>(new EffectPanel(*p,safe->changed,safe->engine.sampleRate()),p->name());}return;}
        if(id==2 && p){p->bypass=!p->bypass.load();safe->changed();safe->refresh();return;}
        safe->closeEditors();
        safe->apply([safe,slot,id,types]{
            if(!safe)return false;auto& insert=safe->chain()[slot];if(insert)insert->closeEditor();
            if(id==3){insert.reset();if(safe->index>=0){auto& lanes=safe->engine.track(safe->index).automation;std::erase_if(lanes,[slot](const auto& lane){return lane.target==slot;});}return true;}
            std::unique_ptr<MixProcessor> replacement;
            if(id>=10&&id<=14){replacement=std::make_unique<MixProcessor>(static_cast<EffectKind>(id-10));replacement->prepare(safe->engine.sampleRate());}
            else if(id>=100&&id-100<types.size()){auto o=std::make_unique<juce::DynamicObject>();o->setProperty("kind",5);o->setProperty("bypass",false);o->setProperty("parameters",juce::Array<juce::var>{});o->setProperty("description",types[id-100].createXml()->toString());o->setProperty("state","");juce::String error;
                replacement=MixProcessor::restore(juce::var(o.release()),safe->engine.sampleRate(),error);if(!replacement || !replacement->plugin){safe->showError(replacement?replacement->failure:error);return false;}}
            if(!replacement)return false;insert=std::move(replacement);
            if(safe->index>=0)std::erase_if(safe->engine.track(safe->index).automation,[slot](const auto& lane){return lane.target==slot;});return true;
        });safe->refresh();
    });
}
void ChannelStrip::sendEditor(int slot){if(index<0)return;closeEditors();toolWindow=std::make_unique<ToolWindow>(new SendPanel(engine,index,slot,apply,changed),"Send "+juce::String(slot+1));}
void ChannelStrip::automationEditor(){if(index<0)return;closeEditors();toolWindow=std::make_unique<ToolWindow>(new AutomationPanel(engine,index,apply),"Automacao Read: "+engine.track(index).name);}
void ChannelStrip::refresh(){
    auto* t=index>=0?&engine.track(index):nullptr;
    if(!name.isBeingEdited())name.setText(t?t->name:"MASTER",juce::dontSendNotification);name.setColour(juce::Label::textColourId,t?juce::Colour(t->colour):accent);
    gain.setValue(juce::Decibels::gainToDecibels(t?t->gain.load():engine.masterGain.load(),-60.0f),juce::dontSendNotification);
    if(t){pan.setValue(t->pan.load(),juce::dontSendNotification);inputGain.setValue(juce::Decibels::gainToDecibels(t->inputGain.load(),-24.0f),juce::dontSendNotification);
        output.setSelectedId(t->outputTarget<0?t->outputMode.load()+1:t->outputTarget+4,juce::dontSendNotification);solo.setToggleState(t->solo,juce::dontSendNotification);automation.setButtonText(t->automationMode==AutomationMode::read?"Auto Read":"Auto Off");}
    input.setText(!t?"OUT: dispositivo stereo":t->auxiliary?(t->monoBus?"IN: bus mono":"IN: bus stereo"):"IN: regioes WAV",juce::dontSendNotification);
    mute.setToggleState(t?t->mute.load():engine.masterMute.load(),juce::dontSendNotification);
    const float peak=t?t->peak.load():engine.masterPeak.load();peakLabel.setText("Peak "+juce::String(juce::Decibels::gainToDecibels(peak,-90.0f),1)+" dB",juce::dontSendNotification);
    clip.setColour(juce::TextButton::buttonColourId,(t?t->clipped.load():engine.masterClipped.load())?juce::Colour(0xffb74658):juce::Colour(0xff283446));
    for(int slot=0;slot<4;++slot){const auto& p=chain()[slot];inserts[slot].setButtonText(juce::String(slot+1)+" "+(p?p->name():"+ insert"));inserts[slot].setAlpha(p&&p->bypass?0.5f:1.0f);}
    if(t)for(int slot=0;slot<2;++slot){const auto& send=t->sends[slot];sends[slot].setButtonText(send.target<0?"+ send "+juce::String(slot+1):(send.preFader?"Pre ":"Post ")+engine.track(send.target).name+" "+juce::String(juce::Decibels::gainToDecibels(send.gain.load(),-60.0f),1));}
    repaint();
}
void ChannelStrip::resized(){const int w=getWidth();name.setBounds(8,6,w-50,24);colour.setBounds(w-42,7,34,22);input.setBounds(8,33,w-16,19);output.setBounds(8,55,w-16,24);inputGain.setBounds(8,82,w-16,27);
    for(int i=0;i<4;++i)inserts[i].setBounds(8,114+i*25,w-16,23);for(int i=0;i<2;++i)sends[i].setBounds(8,219+i*25,w-16,23);
    automation.setBounds(8,271,w-16,23);pan.setBounds(8,299,w-16,70);mute.setBounds(8,373,38,24);solo.setBounds(50,373,38,24);clip.setBounds(w-66,373,58,24);
    peakLabel.setBounds(8,401,w-16,22);gain.setBounds(12,428,w-62,std::max(110,getHeight()-437));}
void ChannelStrip::paint(juce::Graphics& g){g.fillAll(panel);g.setColour(index<0?accent:juce::Colour(engine.track(index).colour));g.fillRect(0,0,getWidth(),3);g.setColour(juce::Colour(0xff334053));g.drawRect(getLocalBounds());
    const float peak=index<0?engine.masterPeak.load():engine.track(index).peak.load(),rms=index<0?engine.masterRms.load():engine.track(index).rms.load();
    meter(g,{getWidth()-42.0f,432,9,std::max(90.0f,getHeight()-470.0f)},peak);meter(g,{getWidth()-28.0f,432,9,std::max(90.0f,getHeight()-470.0f)},rms);
    g.setColour(juce::Colour(0xff8797ab));g.setFont(10);g.drawText("P  RMS",getWidth()-48,getHeight()-33,45,18,juce::Justification::centred);
}
bool ChannelStrip::validatePhase3Bindings(int auxiliary){
    if(index<0)return false;
    apply([this]{chain()[0]=std::make_unique<MixProcessor>(EffectKind::equalizer);return true;});
    auto* effect=new EffectPanel(*chain()[0],changed,engine.sampleRate());toolWindow=std::make_unique<ToolWindow>(effect,"EQ validation");const bool effectOk=effect->validateBindings();closeEditors();
    auto* send=new SendPanel(engine,index,0,apply,changed);toolWindow=std::make_unique<ToolWindow>(send,"Send validation");const bool sendOk=send->validateBindings(auxiliary);closeEditors();
    auto* automationPanel=new AutomationPanel(engine,index,apply);toolWindow=std::make_unique<ToolWindow>(automationPanel,"Automation validation");const bool automationOk=automationPanel->validateBindings();closeEditors();
    // Native editor, send and automation share the actual components used by their buttons.
    return effectOk&&sendOk&&automationOk;
}
bool ChannelStrip::validateBindings(){if(index<0)return false;refresh();auto& t=engine.track(index);const float oldGain=t.gain,oldPan=t.pan,oldInput=t.inputGain;const bool oldMute=t.mute,oldSolo=t.solo;const int oldMode=t.outputMode,oldTarget=t.outputTarget;
    gain.setValue(-6,juce::sendNotificationSync);pan.setValue(1,juce::sendNotificationSync);inputGain.setValue(6,juce::sendNotificationSync);mute.setToggleState(true,juce::sendNotificationSync);solo.setToggleState(true,juce::sendNotificationSync);output.setSelectedId(3,juce::sendNotificationSync);
    const bool ok=std::abs(t.gain.load()-juce::Decibels::decibelsToGain(-6.0f))<1e-5f&&t.pan==1&&t.mute&&t.solo&&t.outputMode==2&&t.inputGain>1.99f;
    apply([&]{t.gain=oldGain;t.pan=oldPan;t.inputGain=oldInput;t.mute=oldMute;t.solo=oldSolo;t.outputMode=oldMode;juce::String error;return engine.setRoute(index,oldTarget,error);});refresh();return ok;
}
}
