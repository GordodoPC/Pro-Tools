#include "MixComponents.h"
#include "MasteringComponents.h"
#include "Session/AutomationModel.h"
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
    EffectPanel(MixProcessor& p,std::function<void()> c,double rate,AudioEngine* owner=nullptr,int track=-1,int slot=-1):processor(p),changed(std::move(c)),sampleRate(rate){
        for(int i=0;i<p.parameterCount();++i){auto slider=std::make_unique<juce::Slider>();const auto spec=p.spec(i);slider->setRange(spec.minimum,spec.maximum,0);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,75,22);slider->setValue(p.parameter(i));
            slider->onValueChange=[this,i,s=slider.get()]{processor.setParameter(i,static_cast<float>(s->getValue()));changed();repaint();};
            if(owner&&track>=0){slider->onDragStart=[owner,track,slot,i]{juce::String error;owner->automation->begin(track,slot,i,error);};slider->onDragEnd=[owner,track,slot,i]{owner->automation->end(track,slot,i);};}
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
        level.onValueChange=[this]{engine.track(index).sends[sendSlot].gain=level.getValue()<=-60?0:juce::Decibels::decibelsToGain(float(level.getValue()));changed();};
        addAndMakeVisible(pan);addAndMakeVisible(width);addAndMakeVisible(mute);pan.setRange(-1,1,0.01);pan.setTextValueSuffix(" pan");width.setRange(0,2,0.01);width.setTextValueSuffix(" width");pan.setValue(send.pan);width.setValue(send.width);mute.setClickingTogglesState(true);mute.setToggleState(send.mute,juce::dontSendNotification);
        pan.onValueChange=[this]{engine.track(index).sends[sendSlot].pan=float(pan.getValue());changed();};width.onValueChange=[this]{engine.track(index).sends[sendSlot].width=float(width.getValue());changed();};mute.onClick=[this]{engine.track(index).sends[sendSlot].mute=mute.getToggleState();changed();};setSize(440,290);
    }
    void resized()override{destination.setBounds(15,15,getWidth()-30,30);pre.setBounds(15,55,160,28);label.setBounds(15,95,getWidth()-30,22);level.setBounds(15,120,getWidth()-30,30);pan.setBounds(15,160,getWidth()-30,30);width.setBounds(15,200,getWidth()-30,30);mute.setBounds(15,245,120,25);}
    bool validateBindings(int auxiliary){destination.setSelectedId(auxiliary+2,juce::sendNotificationSync);pre.setToggleState(true,juce::dontSendNotification);pre.onClick();level.setValue(-6,juce::sendNotificationSync);
        const auto& send=engine.track(index).sends[sendSlot];return send.target==auxiliary&&send.preFader&&std::abs(send.gain.load()-juce::Decibels::decibelsToGain(-6.0f))<1e-5f;}
private:AudioEngine& engine;int index,sendSlot;MixEdit apply;std::function<void()> changed;juce::ComboBox destination;juce::TextButton pre{"Pre-fader"},mute{"Mute send"};juce::Slider level,pan,width;juce::Label label;
};
class GroupPanel final : public juce::Component {
public:
    GroupPanel(AudioEngine& e,int track,MixEdit a,std::function<void()> c):engine(e),index(track),apply(std::move(a)),changed(std::move(c)){
        for(auto* child:std::initializer_list<juce::Component*>{&vca,&group,&sidechain,&kind,&number,&name,&gain,&mute,&solo,&safe,&info})addAndMakeVisible(child);
        vca.addItem("Sem VCA",1);group.addItem("Sem grupo",1);for(int i=0;i<8;++i){vca.addItem("VCA "+juce::String(i+1),i+2);group.addItem("Grupo "+juce::String(i+1),i+2);number.addItem(juce::String(i+1),i+1);}vca.setSelectedId(e.track(index).vcaGroup+2);group.setSelectedId(e.track(index).channelGroup+2);
        vca.onChange=[this]{apply([&]{engine.track(index).vcaGroup=vca.getSelectedId()-2;return true;});};group.onChange=[this]{apply([&]{engine.track(index).channelGroup=group.getSelectedId()-2;return true;});};
        sidechain.addItem("Sidechain: desligado",1);for(int i=0;i<e.trackCount();++i)if(i!=index)sidechain.addItem("Key: "+e.track(i).name,i+2);sidechain.setSelectedId(e.track(index).sidechainSource+2);
        sidechain.onChange=[this]{juce::String error;if(!apply([&]{return engine.setSidechain(index,sidechain.getSelectedId()-2,error);})){sidechain.setSelectedId(engine.track(index).sidechainSource+2,juce::dontSendNotification);info.setText(error,juce::dontSendNotification);}};
        kind.addItem("VCA",1);kind.addItem("Grupo de canais",2);kind.setSelectedId(1);number.setSelectedId(1);kind.onChange=[this]{refresh();};number.onChange=[this]{refresh();};
        gain.setRange(-60,12,0.1);gain.setTextValueSuffix(" dB grupo");gain.onValueChange=[this]{selected().gain=gain.getValue()<=-60?0:juce::Decibels::decibelsToGain(float(gain.getValue()));changed();};
        mute.setClickingTogglesState(true);solo.setClickingTogglesState(true);safe.setClickingTogglesState(true);mute.onClick=[this]{selected().mute=mute.getToggleState();changed();};solo.onClick=[this]{selected().solo=solo.getToggleState();changed();};safe.setToggleState(e.track(index).soloSafe,juce::dontSendNotification);safe.onClick=[this]{engine.track(index).soloSafe=safe.getToggleState();changed();};name.onTextChange=[this]{apply([&]{selected().name=name.getText().substring(0,128);return true;});};
        info.setText("Key externo: NOVA Compressor/Gate. PDC inclui a fonte e os inserts anteriores.\nVCA/grupos afetam pos-fader; sends pre-fader preservam nivel.\nRender/dispositivo atuais: stereo. Matriz 5.1/7.1 preparada na API.",juce::dontSendNotification);setSize(620,430);refresh();
    }
    void resized()override{vca.setBounds(15,15,280,28);group.setBounds(310,15,295,28);sidechain.setBounds(15,55,590,28);safe.setBounds(15,95,200,28);kind.setBounds(15,140,220,28);number.setBounds(245,140,80,28);name.setBounds(15,180,590,28);gain.setBounds(15,220,590,32);mute.setBounds(15,265,120,28);solo.setBounds(145,265,120,28);info.setBounds(15,310,590,100);}
    bool validateBindings(){vca.setSelectedId(2,juce::sendNotificationSync);group.setSelectedId(2,juce::sendNotificationSync);gain.setValue(-6,juce::sendNotificationSync);safe.setToggleState(true,juce::dontSendNotification);safe.onClick();return engine.track(index).vcaGroup==0&&engine.track(index).channelGroup==0&&engine.track(index).soloSafe&&std::abs(engine.vcaGroups[0].gain.load()-juce::Decibels::decibelsToGain(-6.0f))<1e-6f;}
private:
    AudioEngine& engine;int index;MixEdit apply;std::function<void()> changed;juce::ComboBox vca,group,sidechain,kind,number;juce::TextEditor name;juce::Slider gain;juce::TextButton mute{"Mute grupo"},solo{"Solo grupo"},safe{"Solo Safe"};juce::Label info;
    AudioEngine::Group& selected(){return kind.getSelectedId()==1?engine.vcaGroups[std::max(0,number.getSelectedId()-1)]:engine.channelGroups[std::max(0,number.getSelectedId()-1)];}
    void refresh(){auto& g=selected();name.setText(g.name,false);gain.setValue(juce::Decibels::gainToDecibels(g.gain.load(),-60.0f),juce::dontSendNotification);mute.setToggleState(g.mute,juce::dontSendNotification);solo.setToggleState(g.solo,juce::dontSendNotification);solo.setEnabled(kind.getSelectedId()==2);}
};
class AutomationCurve final : public juce::Component {
public:
    AutomationCurve(AudioEngine& e,int i,MixEdit a):engine(e),index(i),apply(std::move(a)){}
    int target=-1,parameter=0,shape=0;
    bool validateEdit(){load();if(draft.empty())return false;const float old=draft.front().value;draft.front().value=(low+high)*0.5f;const float expected=draft.front().value;commit();bool changed=false;for(const auto& lane:engine.track(index).automation)if(lane.target==target&&lane.parameter==parameter)changed=lane.points.front().value==expected;
        const bool undone=apply([&]{return engine.automation->undo();});bool restored=false;for(const auto& lane:engine.track(index).automation)if(lane.target==target&&lane.parameter==parameter)restored=lane.points.front().value==old;return changed&&undone&&restored;}
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff0e141d));if(!dragging)load();const auto box=getLocalBounds().toFloat().reduced(8);g.setColour(juce::Colour(0xff3a4558));g.drawRect(box);juce::Path path;
        AutomationLane lane{target,parameter,draft};for(int x=0;x<int(box.getWidth());++x){const float y=toY(lane.evaluate(x/double(box.getWidth())*span,0));if(x==0)path.startNewSubPath(box.getX(),y);else path.lineTo(box.getX()+x,y);}g.setColour(accent);g.strokePath(path,juce::PathStrokeType(2));for(const auto& p:draft)g.fillEllipse(toX(p.time)-3,toY(p.value)-3,6,6);}
    void mouseDown(const juce::MouseEvent& e)override{load();selected=-1;float distance=12;for(int n=0;n<int(draft.size());++n){const float d=juce::Point<float>(toX(draft[n].time),toY(draft[n].value)).getDistanceFrom(e.position);if(d<distance){distance=d;selected=n;}}
        if(e.mods.isShiftDown()&&selected>=0){draft.erase(draft.begin()+selected);commit();return;}if(selected<0&&draft.size()<4096){draft.push_back(point(e.position));std::sort(draft.begin(),draft.end(),[](auto a,auto b){return a.time<b.time;});for(int n=0;n<int(draft.size());++n)if(std::abs(toX(draft[n].time)-e.position.x)<1){selected=n;break;}}if(selected>=0){draft[selected].curve=shape;dragging=true;}repaint();}
    void mouseDrag(const juce::MouseEvent& e)override{if(!dragging||selected<0)return;auto p=point(e.position);p.time=std::clamp(p.time,selected?draft[selected-1].time+1e-6:0.0,selected+1<int(draft.size())?draft[selected+1].time-1e-6:span);draft[selected]=p;repaint();}
    void mouseUp(const juce::MouseEvent&)override{if(dragging){dragging=false;commit();}}
private:
    AudioEngine& engine;int index;MixEdit apply;std::vector<AutomationPoint> draft;int selected=-1;bool dragging=false;double span=1;float low=0,high=2;
    void load(){span=std::max(1.0,engine.duration());low=target==-2?-1.0f:0;high=target==-2?1.0f:2;if(target>=0&&engine.track(index).inserts[target]){auto spec=engine.track(index).inserts[target]->spec(parameter);low=spec.minimum;high=spec.maximum;}draft.clear();for(const auto& lane:engine.track(index).automation)if(lane.target==target&&lane.parameter==parameter)draft=lane.points;}
    float toX(double time)const{return 8+float(time/span)*(getWidth()-16);}
    float toY(float value)const{return getHeight()-8-(value-low)/(high-low)*(getHeight()-16);}
    AutomationPoint point(juce::Point<float> p)const{return {std::clamp(double((p.x-8)/(getWidth()-16)*span),0.0,span),std::clamp(low+(getHeight()-8-p.y)/(getHeight()-16)*(high-low),low,high),shape};}
    void commit(){auto lanes=engine.track(index).automation;auto found=std::find_if(lanes.begin(),lanes.end(),[&](const auto& l){return l.target==target&&l.parameter==parameter;});if(found==lanes.end())lanes.push_back({target,parameter,draft});else found->points=draft;apply([&,lanes=std::move(lanes)]()mutable{juce::String error;return engine.automation->replace(index,std::move(lanes),engine.track(index).automationMode,error);});repaint();}
};
class AutomationPanel final : public juce::Component {
public:
    AutomationPanel(AudioEngine& e,int i,MixEdit a,std::function<void()> c):engine(e),index(i),apply(std::move(a)),curve(e,i,apply),changed(std::move(c)){
        for(auto* c:std::initializer_list<juce::Component*>{&target,&parameter,&points,&instructions,&commit,&mode,&result})addAndMakeVisible(c);
        target.addItem("Volume (ganho linear 0..2)",1);target.addItem("Pan (-1..1)",2);
        for(int slot=0;slot<4;++slot)if(auto& p=e.track(i).inserts[slot])target.addItem("Insert "+juce::String(slot+1)+": "+p->name(),slot+3);
        target.onChange=[this]{parameter.clear();const int dest=target.getSelectedId()-3;
            if(dest<0)parameter.addItem("Valor",1);else if(auto& p=engine.track(index).inserts[dest])for(int j=0;j<p->parameterCount();++j)parameter.addItem(p->kind==EffectKind::vst3?p->plugin->getParameters()[j]->getName(40):juce::String(p->spec(j).name),j+1);
            parameter.setSelectedId(1,juce::dontSendNotification);loadPoints();};
        parameter.onChange=[this]{loadPoints();};target.setSelectedId(1,juce::sendNotificationSync);
        mode.addItem("Off",1);mode.addItem("Read",2);mode.addItem("Write",3);mode.addItem("Touch",4);mode.addItem("Latch",5);mode.setSelectedId(int(e.track(i).automationMode)+1,juce::dontSendNotification);
        mode.onChange=[this]{apply([&]{juce::String error;return engine.automation->replace(index,engine.track(index).automation,AutomationMode(mode.getSelectedId()-1),error);});};
        for(auto* child:std::initializer_list<juce::Component*>{&curve,&shape,&live,&undo,&redo})addAndMakeVisible(child);
        shape.addItem("Linear",1);shape.addItem("Hold",2);shape.addItem("Smooth",3);shape.setSelectedId(1);shape.onChange=[this]{curve.shape=shape.getSelectedId()-1;};
        undo.onClick=[this]{apply([&]{return engine.automation->undo();});loadPoints();};redo.onClick=[this]{apply([&]{return engine.automation->redo();});loadPoints();};
        live.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,24);live.setRange(0,2);
        live.onDragStart=[this]{juce::String error;engine.automation->begin(index,curve.target,curve.parameter,error);};live.onDragEnd=[this]{engine.automation->end(index,curve.target,curve.parameter);};
        live.onValueChange=[this]{const float v=float(live.getValue());auto& t=engine.track(index);if(curve.target==-1)t.gain=v;else if(curve.target==-2)t.pan=v;else if(t.inserts[curve.target])t.inserts[curve.target]->setParameter(curve.parameter,v);changed();};
        points.setMultiLine(true);points.setReturnKeyStartsNewLine(true);instructions.setText("Curva: clique/arraste pontos; Shift-clique exclui. Undo/Redo apos Stop.\nWrite grava lanes; Touch retorna ao soltar; Latch mantem ate Stop. Slider grava o destino escolhido. Texto: segundos : valor.",juce::dontSendNotification);
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
            const bool applied=apply([this,lane=std::move(lane),selectedMode]()mutable{auto lanes=engine.track(index).automation;auto found=std::find_if(lanes.begin(),lanes.end(),[&](const auto& item){return item.target==lane.target && item.parameter==lane.parameter;});
                if(found==lanes.end()&&lanes.size()>=64&&!lane.points.empty()){result.setText("Limite de 64 lanes por canal.",juce::dontSendNotification);return false;}
                if(found!=lanes.end())lanes.erase(found);for(auto& p:lane.points)p.curve=shape.getSelectedId()-1;if(!lane.points.empty())lanes.push_back(std::move(lane));juce::String error;return engine.automation->replace(index,std::move(lanes),selectedMode,error);});
            if(applied){loadPoints();result.setText("Automacao aplicada.",juce::dontSendNotification);}
        };loadPoints();setSize(720,650);
    }
    void resized()override{target.setBounds(15,15,getWidth()-30,28);parameter.setBounds(15,50,getWidth()-150,28);mode.setBounds(getWidth()-125,50,110,28);
        instructions.setBounds(15,85,getWidth()-30,65);shape.setBounds(15,150,140,25);live.setBounds(165,150,getWidth()-180,25);curve.setBounds(15,185,getWidth()-30,180);points.setBounds(15,375,getWidth()-30,getHeight()-465);undo.setBounds(15,getHeight()-85,120,25);redo.setBounds(145,getHeight()-85,120,25);commit.setBounds(15,getHeight()-55,150,30);result.setBounds(175,getHeight()-55,getWidth()-190,30);}
    bool validateBindings(){target.setSelectedId(1,juce::sendNotificationSync);parameter.setSelectedId(1,juce::dontSendNotification);mode.setSelectedId(2,juce::dontSendNotification);points.setText("0 : 0.5\n1 : 1");commit.onClick();
        const bool edited=curve.validateEdit();const auto& t=engine.track(index);const float oldGain=t.gain;live.setValue(0.75,juce::sendNotificationSync);const bool liveBound=t.gain==0.75f;live.setValue(oldGain,juce::sendNotificationSync);return edited&&liveBound&&t.automationMode==AutomationMode::read&&std::any_of(t.automation.begin(),t.automation.end(),[](const auto& lane){return lane.target==-1&&lane.points.size()==2&&lane.points[0].value==0.5f;});}
private:AudioEngine& engine;int index;MixEdit apply;AutomationCurve curve;std::function<void()> changed;juce::ComboBox target,parameter,mode,shape;juce::Slider live;juce::TextEditor points;juce::Label instructions,result;juce::TextButton commit{"Aplicar lane"},undo{"Undo automacao"},redo{"Redo automacao"};
    void loadPoints(){mode.setSelectedId(int(engine.track(index).automationMode)+1,juce::dontSendNotification);const int selected=target.getSelectedId();const int dest=selected==1?-1:selected==2?-2:selected-3;const int param=parameter.getSelectedId()-1;juce::String text;
        curve.target=dest;curve.parameter=std::max(0,param);float low=dest==-2?-1.0f:0,high=dest==-2?1.0f:2;if(dest>=0&&engine.track(index).inserts[dest]){auto spec=engine.track(index).inserts[dest]->spec(curve.parameter);low=spec.minimum;high=spec.maximum;}live.setRange(low,high);live.setValue(dest==-1?engine.track(index).gain.load():dest==-2?engine.track(index).pan.load():engine.track(index).inserts[dest]?engine.track(index).inserts[dest]->parameter(curve.parameter):0,juce::dontSendNotification);
        for(const auto& lane:engine.track(index).automation)if(lane.target==dest&&lane.parameter==param)for(const auto& p:lane.points)text+=juce::String(p.time,6)+" : "+juce::String(p.value,6)+"\n";points.setText(text);curve.repaint();}
};
void meter(juce::Graphics& g,juce::Rectangle<float> r,float value){g.setColour(juce::Colour(0xff080d14));g.fillRect(r);
    const float amount=std::clamp((juce::Decibels::gainToDecibels(value,-60.0f)+60)/60,0.0f,1.0f);g.setColour(value>1?juce::Colours::salmon:accent);g.fillRect(r.removeFromBottom(r.getHeight()*amount));}
}
std::unique_ptr<juce::DocumentWindow> createPluginManager(PluginCatalog& catalog){return std::make_unique<ToolWindow>(new PluginManagerPanel(catalog),"Plugins VST3");}
ChannelStrip::ChannelStrip(AudioEngine& e,EditModel& ed,int i,PluginCatalog& cat,MixEdit a,std::function<void()> c):engine(e),edit(ed),index(i),catalog(cat),apply(std::move(a)),changed(std::move(c)){
    addAndMakeVisible(groups);groups.onClick=[this]{closeEditors();if(index>=0)toolWindow=std::make_unique<ToolWindow>(new GroupPanel(engine,index,apply,changed),"VCA, grupos e sidechain");else toolWindow=createMasteringEditor(engine,changed);};if(index<0)groups.setButtonText("LUFS / Export");
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
    gain.onDragStart=[this]{if(index>=0){juce::String error;if(!engine.automation->begin(index,-1,0,error))showError(error);}};
    gain.onDragEnd=[this]{if(index>=0)engine.automation->end(index,-1,0);};
    pan.onDragStart=[this]{if(index>=0){juce::String error;if(!engine.automation->begin(index,-2,0,error))showError(error);}};
    pan.onDragEnd=[this]{if(index>=0)engine.automation->end(index,-2,0);};
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
    const auto types=catalog.known.getTypes();if(!types.isEmpty()){juce::PopupMenu vst;for(int i=0;i<types.size();++i)if(!types[i].isInstrument)vst.addItem(100+i,types[i].name);menu.addSubMenu("VST3",vst);}
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&inserts[slot]),[safe=juce::Component::SafePointer<ChannelStrip>(this),slot,types](int id){
        if(!safe || !id)return;auto& p=safe->chain()[slot];
        if(id==1 && p){if(p->kind==EffectKind::vst3){if(p->plugin){AudioEngine::MutationGuard guard(safe->engine);p->openEditor();}else safe->showError(p->failure);}
            else{safe->closeEditors();safe->toolWindow=std::make_unique<ToolWindow>(new EffectPanel(*p,safe->changed,safe->engine.sampleRate(),&safe->engine,safe->index,slot),p->name());}return;}
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
void ChannelStrip::automationEditor(){if(index<0)return;closeEditors();toolWindow=std::make_unique<ToolWindow>(new AutomationPanel(engine,index,apply,changed),"Automacao: "+engine.track(index).name);}
void ChannelStrip::refresh(){
    auto* t=index>=0?&engine.track(index):nullptr;
    if(!name.isBeingEdited())name.setText(t?t->name:"MASTER",juce::dontSendNotification);name.setColour(juce::Label::textColourId,t?juce::Colour(t->colour):accent);
    gain.setValue(juce::Decibels::gainToDecibels(t?t->gain.load():engine.masterGain.load(),-60.0f),juce::dontSendNotification);
    if(t){pan.setValue(t->pan.load(),juce::dontSendNotification);inputGain.setValue(juce::Decibels::gainToDecibels(t->inputGain.load(),-24.0f),juce::dontSendNotification);
        output.setSelectedId(t->outputTarget<0?t->outputMode.load()+1:t->outputTarget+4,juce::dontSendNotification);solo.setToggleState(t->solo,juce::dontSendNotification);static const char* modes[]{"Auto Off","Auto Read","Auto Write","Auto Touch","Auto Latch"};automation.setButtonText(modes[int(t->automationMode)]);}
    input.setText(!t?"OUT: dispositivo stereo":t->midiTrack?"IN: MIDI":t->auxiliary?(t->monoBus?"IN: bus mono":"IN: bus stereo"):"IN: regioes WAV",juce::dontSendNotification);
    mute.setToggleState(t?t->mute.load():engine.masterMute.load(),juce::dontSendNotification);
    const float peak=t?t->peak.load():engine.masterPeak.load();peakLabel.setText("Peak "+juce::String(juce::Decibels::gainToDecibels(peak,-90.0f),1)+" dB",juce::dontSendNotification);
    clip.setColour(juce::TextButton::buttonColourId,(t?t->clipped.load():engine.masterClipped.load())?juce::Colour(0xffb74658):juce::Colour(0xff283446));
    for(int slot=0;slot<4;++slot){const auto& p=chain()[slot];inserts[slot].setButtonText(juce::String(slot+1)+" "+(p?p->name():"+ insert"));inserts[slot].setAlpha(p&&p->bypass?0.5f:1.0f);}
    if(t)for(int slot=0;slot<2;++slot){const auto& send=t->sends[slot];sends[slot].setButtonText(send.target<0?"+ send "+juce::String(slot+1):(send.preFader?"Pre ":"Post ")+engine.track(send.target).name+" "+juce::String(juce::Decibels::gainToDecibels(send.gain.load(),-60.0f),1));}
    repaint();
}
void ChannelStrip::resized(){const int w=getWidth();name.setBounds(8,6,w-50,24);colour.setBounds(w-42,7,34,22);input.setBounds(8,33,w-16,19);output.setBounds(8,55,w-16,24);inputGain.setBounds(8,82,w-16,27);
    for(int i=0;i<4;++i)inserts[i].setBounds(8,114+i*25,w-16,23);for(int i=0;i<2;++i)sends[i].setBounds(8,219+i*25,w-16,23);
    automation.setBounds(8,271,(w-24)/2,23);groups.setBounds(index<0?8:w/2+4,271,index<0?w-16:(w-24)/2,23);pan.setBounds(8,299,w-16,70);mute.setBounds(8,373,38,24);solo.setBounds(50,373,38,24);clip.setBounds(w-66,373,58,24);
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
    bool liveChangeNotified=false;auto* automationPanel=new AutomationPanel(engine,index,apply,[this,&liveChangeNotified]{liveChangeNotified=true;changed();});toolWindow=std::make_unique<ToolWindow>(automationPanel,"Automation validation");const bool automationOk=automationPanel->validateBindings()&&liveChangeNotified;closeEditors();
    // Native editor, send and automation share the actual components used by their buttons.
    return effectOk&&sendOk&&automationOk&&validateProfessionalBindings();
}
bool ChannelStrip::validateProfessionalBindings(){if(index<0)return false;auto& t=engine.track(index);const int oldVca=t.vcaGroup,oldGroup=t.channelGroup;const float oldGain=engine.vcaGroups[0].gain;const bool oldSafe=t.soloSafe;
    auto* content=new GroupPanel(engine,index,apply,changed);toolWindow=std::make_unique<ToolWindow>(content,"VCA / Group validation");const bool passed=content->validateBindings();closeEditors();apply([&]{t.vcaGroup=oldVca;t.channelGroup=oldGroup;t.soloSafe=oldSafe;engine.vcaGroups[0].gain=oldGain;return true;});return passed;}
bool ChannelStrip::validateBindings(){if(index<0)return false;refresh();auto& t=engine.track(index);const float oldGain=t.gain,oldPan=t.pan,oldInput=t.inputGain;const bool oldMute=t.mute,oldSolo=t.solo;const int oldMode=t.outputMode,oldTarget=t.outputTarget;
    gain.setValue(-6,juce::sendNotificationSync);pan.setValue(1,juce::sendNotificationSync);inputGain.setValue(6,juce::sendNotificationSync);mute.setToggleState(true,juce::sendNotificationSync);solo.setToggleState(true,juce::sendNotificationSync);output.setSelectedId(3,juce::sendNotificationSync);
    const bool ok=std::abs(t.gain.load()-juce::Decibels::decibelsToGain(-6.0f))<1e-5f&&t.pan==1&&t.mute&&t.solo&&t.outputMode==2&&t.inputGain>1.99f;
    apply([&]{t.gain=oldGain;t.pan=oldPan;t.inputGain=oldInput;t.mute=oldMute;t.solo=oldSolo;t.outputMode=oldMode;juce::String error;return engine.setRoute(index,oldTarget,error);});refresh();return ok;
}
}
