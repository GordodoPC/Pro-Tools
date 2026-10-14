#include "EditorComponents.h"
#include <cmath>
namespace nova {
namespace {
const juce::Colour bg{0xff10151e},panel{0xff1c2532},line{0xff303b4e},accent{0xff59d9c3};
void meter(juce::Graphics& g,juce::Rectangle<float> r,float level,bool horizontal){
    g.setColour(juce::Colour(0xff070b11));g.fillRoundedRectangle(r,2);
    g.setColour(level>1?juce::Colour(0xffff7079):accent);const auto amount=juce::jlimit(0.0f,1.0f,level);
    if(horizontal)g.fillRect(r.withWidth(r.getWidth()*amount));else g.fillRect(r.removeFromBottom(r.getHeight()*amount));
}
}
TrackControls::TrackControls(AudioEngine& e,EditModel& ed,int i,bool v,ApplyEdit a,std::function<void()> changed)
    :engine(e),editor(ed),index(i),vertical(v),apply(a){
    for(auto* c:std::initializer_list<juce::Component*>{&name,&gainLabel,&panLabel,&gain,&pan,&mute,&solo,&record,&colour,&output,&input})addAndMakeVisible(c);
    name.setEditable(false,true,false);name.setFont(juce::Font(juce::FontOptions(14).withStyle("Bold")));
    name.onTextChange=[this]{const auto text=name.getText();apply([this,text]{editor.rename(index,text);return true;});};
    gainLabel.setText("VOL dB",juce::dontSendNotification);panLabel.setText("PAN",juce::dontSendNotification);
    input.setText(engine.track(index).auxiliary?"IN: bus":"IN: regioes WAV",juce::dontSendNotification);input.setFont(11.0f);
    record.setEnabled(false);record.setTooltip("Record Arm pendente: o motor ainda nao grava.");
    gain.setRange(-60,6,0.1);gain.setDoubleClickReturnValue(true,0);
    gain.setSliderStyle(v?juce::Slider::LinearVertical:juce::Slider::LinearHorizontal);
    gain.setTextBoxStyle(v?juce::Slider::TextBoxBelow:juce::Slider::TextBoxRight,false,56,18);
    pan.setRange(-1,1,0.01);pan.setDoubleClickReturnValue(true,0);
    pan.setSliderStyle(v?juce::Slider::RotaryHorizontalVerticalDrag:juce::Slider::LinearHorizontal);
    pan.setTextBoxStyle(v?juce::Slider::TextBoxBelow:juce::Slider::TextBoxRight,false,56,18);
    mute.setClickingTogglesState(true);solo.setClickingTogglesState(true);
    mute.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffae555d));
    solo.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffaf8b35));
    gain.onValueChange=[this,changed]{engine.track(index).gain=gain.getValue()<=-60?0:juce::Decibels::decibelsToGain(static_cast<float>(gain.getValue()));changed();};
    pan.onValueChange=[this,changed]{engine.track(index).pan=static_cast<float>(pan.getValue());changed();};
    mute.onClick=[this,changed]{engine.track(index).mute=mute.getToggleState();changed();};
    solo.onClick=[this,changed]{engine.track(index).solo=solo.getToggleState();changed();};
    output.addItem("Master stereo",1);output.addItem("Master L",2);output.addItem("Master R",3);
    for(int j=0;j<engine.trackCount();++j)if(j!=index && engine.track(j).auxiliary)output.addItem(engine.track(j).name,j+4);
    output.onChange=[this]{const int id=output.getSelectedId();juce::String error;if(!apply([&,id]{if(!engine.setRoute(index,id<=3?-1:id-4,error))return false;engine.track(index).outputMode=id<=3?id-1:0;return true;}))juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Roteamento",error);};
    colour.onClick=[this]{
        juce::PopupMenu menu;const juce::StringArray labels{"Teal","Blue","Violet","Pink","Orange","Green"};
        for(int id=1;id<=6;++id)menu.addItem(id,labels[id-1]);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&colour),[safe=juce::Component::SafePointer<TrackControls>(this)](int id){
            if(!safe || id<1)return;static constexpr uint32_t palette[]={0xff59d9c3,0xff74a9f4,0xffaa8cec,0xffeb87b3,0xffe4a362,0xff9fce76};
            const auto value=palette[id-1];safe->apply([safe,value]{if(!safe)return false;safe->editor.recolour(safe->index,value);return true;});
        });
    };refresh();
}
void TrackControls::refresh(){
    auto& t=engine.track(index);
    if(!name.isBeingEdited())name.setText(t.name,juce::dontSendNotification);
    name.setColour(juce::Label::textColourId,juce::Colour(t.colour));
    gain.setValue(juce::Decibels::gainToDecibels(t.gain.load(),-60.0f),juce::dontSendNotification);
    pan.setValue(t.pan.load(),juce::dontSendNotification);
    mute.setToggleState(t.mute.load(),juce::dontSendNotification);solo.setToggleState(t.solo.load(),juce::dontSendNotification);
    output.setSelectedId(t.outputTarget<0?t.outputMode.load()+1:t.outputTarget+4,juce::dontSendNotification);repaint();
}
bool TrackControls::validateBindings(){refresh();
    auto& t=engine.track(index);const auto oldGain=t.gain.load(),oldPan=t.pan.load();const auto oldRoute=t.outputMode.load();const bool oldMute=t.mute.load(),oldSolo=t.solo.load();
    gain.setValue(-6,juce::sendNotificationSync);pan.setValue(1,juce::sendNotificationSync);
    mute.setToggleState(true,juce::sendNotificationSync);solo.setToggleState(true,juce::sendNotificationSync);output.setSelectedId(3,juce::sendNotificationSync);
    const bool ok=std::abs(t.gain.load()-juce::Decibels::decibelsToGain(-6.0f))<1e-5f&&t.pan==1&&t.mute&&t.solo&&t.outputMode==2;
    t.gain=oldGain;t.pan=oldPan;t.mute=oldMute;t.solo=oldSolo;t.outputMode=oldRoute;refresh();return ok;
}
void TrackControls::mouseDown(const juce::MouseEvent& e){
    editor.select(index,editor.selectedTrack==index?editor.selectedRegion:0);
    if(e.mods.isPopupMenu()){juce::PopupMenu p;p.addItem(1,"Faixa compacta");p.addItem(2,"Faixa normal");p.addItem(3,"Faixa ampliada");
        p.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe=juce::Component::SafePointer<TrackControls>(this)](int id){if(safe&&id){const int h=id==1?100:id==2?140:240;safe->apply([safe,h]{if(!safe)return false;safe->editor.resizeTrack(safe->index,h);return true;});}});}
}
void TrackControls::paint(juce::Graphics& g){
    g.fillAll(panel);g.setColour(line);g.drawRect(getLocalBounds());g.setColour(juce::Colour(engine.track(index).colour));g.fillRect(0,0,4,getHeight());
    if(editor.selectedTrack==index){g.setColour(accent);g.drawRect(getLocalBounds(),2);}
    if(vertical){g.setColour(juce::Colour(0xff263244));g.fillRoundedRectangle(8,114,getWidth()-16.0f,28,3);g.fillRoundedRectangle(8,146,getWidth()-16.0f,28,3);
        g.setColour(juce::Colour(0xff8b98ac));g.setFont(11.0f);g.drawText(juce::String::fromUTF8("INSERTS · pendente"),10,114,getWidth()-20,28,juce::Justification::centredLeft);
        g.drawText(juce::String::fromUTF8("SENDS · pendente"),10,146,getWidth()-20,28,juce::Justification::centredLeft);}
    meter(g,vertical?juce::Rectangle<float>(getWidth()-20.0f,290,7,getHeight()-315.0f):juce::Rectangle<float>(8,getHeight()-8.0f,getWidth()-16.0f,4),engine.track(index).peak.load(),!vertical);
}
void TrackControls::resized(){
    const int w=getWidth();name.setBounds(8,5,w-56,25);colour.setBounds(w-48,7,40,22);
    mute.setBounds(8,34,32,23);solo.setBounds(44,34,32,23);record.setBounds(80,34,42,23);
    if(vertical){input.setBounds(8,60,w-16,16);output.setBounds(8,79,w-16,26);panLabel.setBounds(8,182,80,16);pan.setBounds(14,198,w-38,70);
        gainLabel.setBounds(8,274,90,17);gain.setBounds(18,292,w-48,std::max(55,getHeight()-310));}
    else {input.setVisible(getHeight()>=160);input.setBounds(8,getHeight()-28,w-16,16);
        output.setBounds(8,61,w-16,22);gainLabel.setBounds(8,87,50,16);gain.setBounds(58,83,w-66,25);
        panLabel.setBounds(8,113,50,16);pan.setBounds(58,109,w-66,25);
        const bool compact=getHeight()<140;gainLabel.setVisible(!compact);panLabel.setVisible(!compact);gain.setVisible(!compact);pan.setVisible(!compact);}
}
TrackPanel::TrackPanel(AudioEngine& e,EditModel& ed,ApplyEdit a):engine(e),editor(ed),apply(a){}
void TrackPanel::rebuild(std::function<void()> changed){controls.clear();for(int i=0;i<engine.trackCount();++i){auto c=std::make_unique<TrackControls>(engine,editor,i,false,apply,changed);addAndMakeVisible(*c);controls.push_back(std::move(c));}layoutRows(getWidth());}
void TrackPanel::layoutRows(int width){int y=0;for(int i=0;i<engine.trackCount();++i){const int h=std::max(100,static_cast<int>(engine.track(i).height*verticalScale));controls[static_cast<size_t>(i)]->setBounds(0,y,width,h);y+=h;}setSize(width,std::max(500,y));}
void TrackPanel::refresh(){for(auto& c:controls)c->refresh();}
bool TrackPanel::validateBindings(){return !controls.empty()&&controls.front()->validateBindings();}
Timeline::Timeline(AudioEngine& e,EditModel& ed,ApplyEdit a):engine(e),editor(ed),apply(a){setWantsKeyboardFocus(true);}
int Timeline::rowHeight(int i)const{return std::max(100,static_cast<int>(engine.track(i).height*verticalScale));}
int Timeline::rowTop(int i)const{int y=0;for(int n=0;n<i;++n)y+=rowHeight(n);return y;}
int Timeline::trackAt(int y)const{for(int i=0;i<engine.trackCount();++i)if(y>=rowTop(i)&&y<rowTop(i)+rowHeight(i))return i;return -1;}
juce::Rectangle<float> Timeline::boundsFor(int i,const Track::Region& r)const{return {static_cast<float>(r.start*pixelsPerSecond),rowTop(i)+10.0f,static_cast<float>(r.length*pixelsPerSecond),rowHeight(i)-20.0f};}
void Timeline::setZoom(double zoom){pixelsPerSecond=zoom;updateSize();}
void Timeline::updateSize(){setSize(std::max(1100,static_cast<int>(std::min(2000000.0,std::max(15.0,engine.duration()+5)*pixelsPerSecond))),std::max(500,rowTop(engine.trackCount())));repaint();}
void Timeline::paint(juce::Graphics& g){
    g.fillAll(bg);const auto visible=g.getClipBounds();const double beat=60/engine.bpm*(4.0/engine.beatUnit),step=beat*editor.gridBeats;
    const bool fine=step*pixelsPerSecond>=8;const double drawStep=fine?step:beat*engine.beatsPerBar;
    const int first=std::max(0,static_cast<int>(visible.getX()/pixelsPerSecond/drawStep));const int last=static_cast<int>(visible.getRight()/pixelsPerSecond/drawStep)+1;
    for(int n=first;n<=last;++n){const int x=static_cast<int>(n*drawStep*pixelsPerSecond);const bool bar=std::abs(std::fmod(n*drawStep,beat*engine.beatsPerBar))<1e-6;
        g.setColour(bar?line.brighter(0.1f):line.withAlpha(0.45f));g.drawVerticalLine(x,0,static_cast<float>(getHeight()));}
    for(int i=0;i<engine.trackCount();++i){auto& t=engine.track(i);const int y=rowTop(i),h=rowHeight(i);
        g.setColour(line);g.drawHorizontalLine(y+h-1,0,static_cast<float>(getWidth()));
        for(const auto& r:t.regions){auto box=boundsFor(i,r);if(!box.intersects(visible.toFloat()))continue;
            const bool selected=editor.selectedRegion==r.id;const auto colour=juce::Colour(t.colour);
            g.setColour(colour.withAlpha(selected?0.34f:0.17f));g.fillRoundedRectangle(box,3);g.setColour(selected?accent:colour);g.drawRoundedRectangle(box,3,selected?2.0f:1.0f);
            g.setFont(12.0f);g.drawText(r.asset->name,box.toNearestInt().reduced(6).removeFromTop(18),juce::Justification::centredLeft);
            const int left=std::max(visible.getX(),static_cast<int>(box.getX())),right=std::min(visible.getRight(),static_cast<int>(box.getRight()));
            const float centre=box.getCentreY()+8,amplitude=std::max(12.0f,box.getHeight()*0.32f);
            for(int x=left;x<right;++x){if(r.asset->waveform.empty())break;const double src=(x/pixelsPerSecond-r.start+r.offset)/r.asset->duration();
                const auto bin=std::min(r.asset->waveform.size()-1,static_cast<size_t>(std::max(0.0,src)*r.asset->waveform.size()));
                const auto p=r.asset->waveform[bin];g.drawVerticalLine(x,centre-p.high*amplitude,centre-p.low*amplitude+1);}
        }
    }
    if(editor.rangeEnd>editor.rangeStart){g.setColour(accent.withAlpha(0.10f));g.fillRect(static_cast<float>(editor.rangeStart*pixelsPerSecond),0.0f,static_cast<float>((editor.rangeEnd-editor.rangeStart)*pixelsPerSecond),static_cast<float>(getHeight()));}
    if(dragging&&!resizing&&tool!=Tool::selector){auto ghost=dragRegion;ghost.start=previewStart;ghost.length=previewEnd-previewStart;
        g.setColour(accent.withAlpha(0.5f));g.drawRoundedRectangle(boundsFor(previewTrack,ghost),3,2);}
    g.setColour(accent);g.drawVerticalLine(static_cast<int>(engine.position()*pixelsPerSecond),0,static_cast<float>(getHeight()));
    if(engine.trackCount()==0){g.setColour(juce::Colour(0xff8c9bad));g.setFont(18.0f);g.drawText("Importe WAV ou arraste arquivos para criar tracks",20,100,650,40,juce::Justification::centredLeft);}
}
void Timeline::mouseDown(const juce::MouseEvent& e){
    grabKeyboardFocus();dragTrack=trackAt(e.y);if(dragTrack<0)return;
    resizing=e.y>=rowTop(dragTrack)+rowHeight(dragTrack)-6;
    if(resizing){dragging=true;dragOrigin=e.position.toInt();return;}
    const double time=editor.quantize(e.x/pixelsPerSecond);engine.seek(time);
    Track::Region* hit=nullptr;for(auto& r:engine.track(dragTrack).regions)if(boundsFor(dragTrack,r).contains(e.position)){hit=&r;break;}
    editor.select(dragTrack,hit?hit->id:0);if(selectionChanged)selectionChanged();
    if(e.mods.isPopupMenu()){juce::PopupMenu p;p.addItem(20,"Copy",hit!=nullptr);p.addItem(21,"Paste",editor.canPaste());p.addItem(22,"Delete",hit!=nullptr);p.addItem(23,"Split at cursor",hit!=nullptr);p.addItem(24,"Loop selected clip",hit!=nullptr);
        p.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe=juce::Component::SafePointer<Timeline>(this)](int id){if(safe&&id&&safe->command)safe->command(id);});return;}
    if(tool==Tool::selector){dragging=true;dragOrigin=e.position.toInt();editor.rangeStart=editor.rangeEnd=time;return;}
    if(!hit)return;
    if(tool==Tool::split){apply([this,time]{return editor.split(time);});return;}
    dragRegion=*hit;dragOrigin=e.position.toInt();previewStart=hit->start;previewEnd=hit->end();previewTrack=dragTrack;
    const auto box=boundsFor(dragTrack,*hit);leftEdge=(tool==Tool::trim||tool==Tool::smart)&&e.x<box.getX()+10;
    rightEdge=(tool==Tool::trim||tool==Tool::smart)&&(e.x>box.getRight()-10 || (tool==Tool::trim&&!leftEdge));dragging=true;
}
void Timeline::mouseDrag(const juce::MouseEvent& e){
    if(!dragging||resizing)return;const double time=editor.quantize(e.x/pixelsPerSecond);
    if(tool==Tool::selector){const double initial=editor.quantize(dragOrigin.x/pixelsPerSecond);editor.rangeStart=std::min(initial,time);editor.rangeEnd=std::max(initial,time);repaint();return;}
    if(leftEdge){previewStart=std::clamp(time,std::max(0.0,dragRegion.start-dragRegion.offset),dragRegion.end()-1/dragRegion.asset->sampleRate);}
    else if(rightEdge){previewEnd=std::clamp(time,dragRegion.start+1/dragRegion.asset->sampleRate,dragRegion.start+dragRegion.asset->duration()-dragRegion.offset);}
    else {previewStart=editor.quantize(dragRegion.start+(e.x-dragOrigin.x)/pixelsPerSecond);previewEnd=previewStart+dragRegion.length;const int target=trackAt(e.y);if(target>=0)previewTrack=target;}
    repaint();
}
void Timeline::mouseUp(const juce::MouseEvent& e){
    if(!dragging)return;dragging=false;
    if(resizing){resizing=false;const int height=engine.track(dragTrack).height+static_cast<int>((e.y-dragOrigin.y)/verticalScale);apply([this,height]{editor.resizeTrack(dragTrack,height);return true;});}
    else if(tool==Tool::selector){if(selectionChanged)selectionChanged();}
    else if(leftEdge||rightEdge)apply([this]{return editor.trim(previewStart,previewEnd);});
    else apply([this]{return editor.move(previewStart,previewTrack);});
    repaint();
}
void Timeline::mouseMove(const juce::MouseEvent& e){const int i=trackAt(e.y);setMouseCursor(i>=0&&e.y>=rowTop(i)+rowHeight(i)-6?juce::MouseCursor::UpDownResizeCursor:tool==Tool::selector?juce::MouseCursor::IBeamCursor:juce::MouseCursor::NormalCursor);}
Ruler::Ruler(AudioEngine& e,EditModel& ed,Timeline& t):engine(e),editor(ed),timeline(t){}
void Ruler::paint(juce::Graphics& g){
    g.fillAll(panel);const double px=timeline.pixelsPerSecond,beat=60/engine.bpm*(4.0/engine.beatUnit);
    const int barStride=std::max(1,static_cast<int>(std::ceil(75/(px*beat*engine.beatsPerBar))));
    for(int bar=static_cast<int>(scrollX/px/beat/engine.beatsPerBar);bar<static_cast<int>((scrollX+getWidth())/px/beat/engine.beatsPerBar)+2;bar+=barStride){
        const int x=static_cast<int>(bar*beat*engine.beatsPerBar*px)-scrollX;g.setColour(juce::Colour(0xffc2cbdb));g.setFont(12.0f);
        g.drawText(juce::String(bar+1)+"|1",x+4,0,70,22,juce::Justification::centredLeft);g.setColour(line);g.drawVerticalLine(x,0,22);}
    const int step=std::max(1,static_cast<int>(std::ceil(75/px)));
    for(int sec=std::max(0,static_cast<int>(scrollX/px)/step*step);sec<static_cast<int>((scrollX+getWidth())/px)+step;sec+=step){const int x=static_cast<int>(sec*px)-scrollX;
        g.setColour(juce::Colour(0xff8b9aae));g.drawText(juce::String(sec)+"s",x+4,23,70,22,juce::Justification::centredLeft);}
    if(engine.loopEnabled){g.setColour(accent.withAlpha(0.18f));g.fillRect(static_cast<float>(engine.loopStart.load()*px-scrollX),0.0f,static_cast<float>((engine.loopEnd.load()-engine.loopStart.load())*px),static_cast<float>(getHeight()));}
    g.setColour(accent);g.drawVerticalLine(static_cast<int>(engine.position()*px)-scrollX,0,static_cast<float>(getHeight()));
}
void Ruler::mouseDown(const juce::MouseEvent& e){engine.seek(editor.quantize((e.x+scrollX)/timeline.pixelsPerSecond));}
Mixer::Mixer(AudioEngine& e,EditModel& ed,ApplyEdit a):engine(e),editor(ed),apply(a),catalog(juce::File(NOVA_PROJECT_ROOT).getChildFile(".nova-plugin-list.xml")){
    for(auto* c:std::initializer_list<juce::Component*>{&addAux,&addMono,&scan,&scanFolder,&scanStatus})addAndMakeVisible(c);
    addAux.onClick=[this]{if(onAddAux)onAddAux(false);};addMono.onClick=[this]{if(onAddAux)onAddAux(true);};
    scan.onClick=[this]{juce::VST3PluginFormat format;catalog.scan(format.getDefaultLocationsToSearch());};
    scanFolder.onClick=[this]{if(onScanFolder)onScanFolder();};
}
void Mixer::rebuild(std::function<void()> changed){controls.clear();for(int i=0;i<=engine.trackCount();++i){auto c=std::make_unique<ChannelStrip>(engine,editor,i==engine.trackCount()?-1:i,catalog,apply,changed);addAndMakeVisible(*c);controls.push_back(std::move(c));}layoutChannels(getHeight());}
void Mixer::layoutChannels(int height){const int h=std::max(650,height);setSize(std::max(1000,(engine.trackCount()+1)*184+20),h);
    addAux.setBounds(8,6,115,28);addMono.setBounds(129,6,115,28);scan.setBounds(250,6,115,28);scanFolder.setBounds(371,6,115,28);scanStatus.setBounds(495,6,490,28);
    for(int i=0;i<int(controls.size());++i)controls[i]->setBounds(8+i*184,42,176,h-50);
}
void Mixer::refresh(){for(auto& c:controls)c->refresh();scanStatus.setText(catalog.status(),juce::dontSendNotification);scan.setEnabled(!catalog.scanning());scanFolder.setEnabled(!catalog.scanning());repaint();}
void Mixer::closeEditors(){for(auto& c:controls)c->closeEditors();engine.closeEditors();}
void Mixer::paint(juce::Graphics& g){g.fillAll(bg);}
bool Mixer::validateBindings(){return !controls.empty()&&controls.front()->validateBindings();}
ClipPanel::ClipPanel(AudioEngine& e,EditModel& ed):engine(e),editor(ed),list("Clips",this){addAndMakeVisible(list);list.setColour(juce::ListBox::backgroundColourId,bg);list.setRowHeight(46);}
void ClipPanel::refresh(){rows.clear();for(int i=0;i<engine.trackCount();++i)for(const auto& r:engine.track(i).regions)rows.emplace_back(i,r.id);list.updateContent();list.repaint();}
void ClipPanel::paint(juce::Graphics& g){g.fillAll(panel);g.setColour(juce::Colours::white);g.setFont(14.0f);g.drawText("CLIPS  /  "+juce::String(rows.size()),10,0,getWidth()-20,35,juce::Justification::centredLeft);}
void ClipPanel::resized(){list.setBounds(getLocalBounds().withTrimmedTop(36));}
int ClipPanel::getNumRows(){return static_cast<int>(rows.size());}
void ClipPanel::paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool selected){if(row<0||row>=getNumRows())return;const auto [i,id]=rows[static_cast<size_t>(row)];
    for(const auto& r:engine.track(i).regions)if(r.id==id){if(selected){g.setColour(line);g.fillAll();}g.setColour(juce::Colour(engine.track(i).colour));g.setFont(12.0f);g.drawText(r.asset->name,8,1,w-12,h/2,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff91a0b4));g.setFont(11.0f);g.drawText(engine.track(i).name+juce::String::fromUTF8(" · ")+juce::String(r.start,2)+"s",8,h/2,w-12,h/2,juce::Justification::centredLeft);}}
void ClipPanel::listBoxItemClicked(int row,const juce::MouseEvent&){if(row<0||row>=getNumRows())return;const auto [i,id]=rows[static_cast<size_t>(row)];editor.select(i,id);if(auto* r=editor.selected())engine.seek(r->start);if(selected)selected();}
}
