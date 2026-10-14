#pragma once
#include "Session/EditModel.h"
namespace nova {
using ApplyEdit = std::function<bool(std::function<bool()>)>;
enum class Tool { smart, grabber, trim, selector, split };
class TrackControls final : public juce::Component {
public:
    TrackControls(AudioEngine&,EditModel&,int,bool,ApplyEdit,std::function<void()>);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void refresh();
    bool validateBindings();
private:
    AudioEngine& engine; EditModel& editor; int index; bool vertical; ApplyEdit apply;
    juce::Label name,gainLabel,panLabel,input;
    juce::Slider gain,pan;
    juce::TextButton mute{"M"},solo{"S"},record{"REC"},colour{"Color"};
    juce::ComboBox output;
};
class TrackPanel final : public juce::Component {
public:
    TrackPanel(AudioEngine&,EditModel&,ApplyEdit);
    double verticalScale=1;
    void rebuild(std::function<void()>);
    void layoutRows(int width);
    void refresh();
    bool validateBindings();
private:
    AudioEngine& engine;EditModel& editor;ApplyEdit apply;
    std::vector<std::unique_ptr<TrackControls>> controls;
};
class Timeline final : public juce::Component {
public:
    Timeline(AudioEngine&,EditModel&,ApplyEdit);
    Tool tool=Tool::smart;
    double pixelsPerSecond=90,verticalScale=1;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void setZoom(double);
    void updateSize();
    int rowTop(int) const;
    int rowHeight(int) const;
    int trackAt(int y) const;
    std::function<void()> selectionChanged;
    std::function<void(int)> command;
private:
    AudioEngine& engine;EditModel& editor;ApplyEdit apply;
    bool dragging=false,leftEdge=false,rightEdge=false,resizing=false;
    int dragTrack=-1;Track::Region dragRegion;juce::Point<int> dragOrigin;
    double previewStart=0,previewEnd=0;
    int previewTrack=-1;
    juce::Rectangle<float> boundsFor(int,const Track::Region&) const;
};
class Ruler final : public juce::Component {
public:
    Ruler(AudioEngine&,EditModel&,Timeline&);
    int scrollX=0;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
private:AudioEngine& engine;EditModel& editor;Timeline& timeline;
};
class Mixer final : public juce::Component {
public:
    Mixer(AudioEngine&,EditModel&,ApplyEdit);
    void rebuild(std::function<void()>);
    void layoutChannels(int height);
    void refresh();
    void paint(juce::Graphics&) override;
    bool validateBindings();
private:
    AudioEngine& engine;EditModel& editor;ApplyEdit apply;
    std::vector<std::unique_ptr<TrackControls>> controls;
    juce::Slider master;juce::Label masterName;
};
class ClipPanel final : public juce::Component,private juce::ListBoxModel {
public:
    ClipPanel(AudioEngine&,EditModel&);
    std::function<void()> selected;
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    AudioEngine& engine;EditModel& editor;juce::ListBox list;
    std::vector<std::pair<int,uint64_t>> rows;
    int getNumRows() override;
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void listBoxItemClicked(int,const juce::MouseEvent&) override;
};
class LinkedViewport final : public juce::Viewport {
public:std::function<void(juce::Rectangle<int>)> onScroll;
private:void visibleAreaChanged(const juce::Rectangle<int>& r) override {if(onScroll)onScroll(r);}
};
class Divider final : public juce::Component {
public:
    std::function<void(int)> onDrag;
    Divider(){setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);}
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff354054));}
    void mouseDown(const juce::MouseEvent& e)override{start=e.getScreenX();}
    void mouseDrag(const juce::MouseEvent& e)override{if(onDrag)onDrag(e.getScreenX()-start);start=e.getScreenX();}
private:int start=0;
};
}
