#pragma once
#include "Audio/MixProcessor.h"
namespace nova {
class AudioEngine;
// Message/control thread owns capture and history. Audio only reads immutable
// lanes and an atomic override mask; no capture allocation happens in render.
class AutomationModel {
public:
    explicit AutomationModel(AudioEngine& owner):engine(owner){}
    bool replace(int track,std::vector<AutomationLane>,AutomationMode,juce::String&);
    bool begin(int track,int target,int parameter,juce::String&);
    void end(int track,int target,int parameter);
    void service();
    void finishAll();
    void reset();
    bool undo();
    bool redo();
    uint64_t overflows()const{return overflowCount;}
    uint64_t revision()const{return changes;}
private:
    AudioEngine& engine;
    struct Capture {bool active=false;double last=0;std::vector<AutomationLane> before,pending;AutomationMode mode=AutomationMode::off;};
    struct Change {int track;std::vector<AutomationLane> before,after;AutomationMode oldMode,newMode;};
    std::array<Capture,64> captures;
    std::vector<Change> past,future;
    uint64_t generation=0,overflowCount=0;
    uint64_t changes=0;
    size_t recordedPoints=0;
    void finish(int);
    bool start(int,juce::String&);
    void remember(Change);
    float value(int,const AutomationLane&)const;
};
}
