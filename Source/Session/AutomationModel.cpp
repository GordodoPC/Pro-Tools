#include "AutomationModel.h"
#include "Audio/AudioEngine.h"
#include <set>
#include <cmath>
namespace nova {
namespace {
size_t laneBytes(const std::vector<AutomationLane>& lanes){size_t bytes=lanes.size()*sizeof(AutomationLane);for(const auto& lane:lanes)bytes+=lane.points.size()*sizeof(AutomationPoint);return bytes;}
bool valid(const Track& track,const std::vector<AutomationLane>& lanes){
    if(lanes.size()>64)return false;std::set<std::pair<int,int>> keys;
    for(const auto& lane:lanes){if(lane.target<-2||lane.target>3||lane.parameter<0||!keys.insert({lane.target,lane.parameter}).second||lane.points.size()>4096)return false;
        float low=lane.target==-2?-1.0f:0.0f,high=lane.target==-2?1.0f:2.0f;
        if(lane.target>=0){const auto& p=track.inserts[lane.target];if(!p||lane.parameter>=p->parameterCount())return false;auto spec=p->spec(lane.parameter);low=spec.minimum;high=spec.maximum;}
        double previous=-1;for(const auto& point:lane.points){if(!std::isfinite(point.time)||!std::isfinite(point.value)||point.time<=previous||point.time<0||point.time>86400||point.value<low||point.value>high||point.curve<0||point.curve>2)return false;previous=point.time;}}
    return true;
}
}
void AutomationModel::reset(){for(auto& capture:captures)capture={};past.clear();future.clear();recordedPoints=0;generation=engine.midiGeneration();}
void AutomationModel::remember(Change change){++changes;future.clear();past.push_back(std::move(change));size_t bytes=0;for(const auto& c:past)bytes+=laneBytes(c.before)+laneBytes(c.after);while(past.size()>32||(bytes>64*1024*1024&&past.size()>1)){bytes-=laneBytes(past.front().before)+laneBytes(past.front().after);past.erase(past.begin());}}
bool AutomationModel::replace(int index,std::vector<AutomationLane> lanes,AutomationMode mode,juce::String& error){
    error.clear();if(generation!=engine.midiGeneration())reset();if(index<0||index>=engine.trackCount()||int(mode)<0||int(mode)>4||!valid(engine.track(index),lanes)){error="Automacao invalida ou limite excedido.";return false;}
    if(captures[index].active){error="Pare a captura de automacao antes de editar a curva.";return false;}
    AudioEngine::MutationGuard guard(engine);auto& track=engine.track(index);remember({index,track.automation,lanes,track.automationMode,mode});track.automation=std::move(lanes);track.automationMode=mode;track.automationOverrideMask=0;return true;
}
bool AutomationModel::start(int index,juce::String& error){auto& capture=captures[index];if(capture.active)return true;auto& t=engine.track(index);
    if(laneBytes(t.automation)>8*1024*1024){error="Automacao excede budget de captura.";return false;}capture.before=t.automation;capture.pending=t.automation;for(auto& lane:capture.pending)lane.points.clear();capture.last=engine.position();capture.mode=t.automationMode;capture.active=true;return true;}
bool AutomationModel::begin(int index,int target,int parameter,juce::String& error){
    error.clear();if(generation!=engine.midiGeneration())reset();if(index<0||index>=engine.trackCount())return false;auto& t=engine.track(index);
    if(t.automationMode==AutomationMode::off||t.automationMode==AutomationMode::read||!engine.isPlaying())return true;
    AutomationLane lane{target,parameter,{}};if(!valid(t,{lane})){error="Destino de automacao indisponivel.";return false;}AudioEngine::MutationGuard guard(engine);if(!start(index,error))return false;
    auto found=std::find_if(t.automation.begin(),t.automation.end(),[&](const auto& l){return l.target==target&&l.parameter==parameter;});
    if(found==t.automation.end()){if(t.automation.size()>=64){error="Limite de 64 lanes.";return false;}t.automation.push_back(lane);captures[index].pending.push_back(lane);found=t.automation.end()-1;}
    t.automationOverrideMask.fetch_or(uint64_t(1)<<size_t(found-t.automation.begin()));return true;
}
float AutomationModel::value(int index,const AutomationLane& lane)const{const auto& t=engine.track(index);return lane.target==-1?t.gain.load():lane.target==-2?t.pan.load():t.inserts[lane.target]?t.inserts[lane.target]->parameter(lane.parameter):0;}
void AutomationModel::service(){
    if(generation!=engine.midiGeneration()){reset();return;}
    for(int index=0;index<engine.trackCount();++index){auto& t=engine.track(index);auto& capture=captures[index];juce::String error;
        if(engine.isPlaying()&&t.automationMode==AutomationMode::write&&!capture.active)start(index,error);
        if(!capture.active)continue;const double now=engine.position();if(!engine.isPlaying()||now+1e-8<capture.last||capture.mode!=t.automationMode){finish(index);continue;}
        const uint64_t mask=t.automationOverrideMask.load();bool overflow=false;
        for(size_t slot=0;slot<capture.pending.size();++slot)if(t.automationMode==AutomationMode::write||(mask&(uint64_t(1)<<slot))){auto& lane=capture.pending[slot];auto& points=lane.points;const float v=value(index,lane);
            if(points.empty()||(now>points.back().time&&v!=points.back().value)){
                if(points.size()>=4094||recordedPoints>=65536){overflow=true;break;}
                // Keep the previous held value up to the previous UI sampling time.
                if(!points.empty()&&capture.last>points.back().time&&points.size()<4093){points.push_back({capture.last,points.back().value});++recordedPoints;}
                points.push_back({now,v});++recordedPoints;
            }
        }
        capture.last=now;if(overflow){++overflowCount;finish(index);AudioEngine::MutationGuard guard(engine);t.automationMode=AutomationMode::read;}
    }
}
void AutomationModel::end(int index,int target,int parameter){if(index<0||index>=engine.trackCount()||!captures[index].active)return;service();if(!captures[index].active)return;auto& t=engine.track(index);
    if(t.automationMode==AutomationMode::touch){finish(index);} // latch/write remain until stop
    (void)target;(void)parameter;
}
void AutomationModel::finish(int index){auto& capture=captures[index];if(!capture.active)return;AudioEngine::MutationGuard guard(engine);auto& t=engine.track(index);auto result=t.automation;bool changed=false;
    for(const auto& written:capture.pending){if(written.points.empty())continue;auto found=std::find_if(result.begin(),result.end(),[&](const auto& l){return l.target==written.target&&l.parameter==written.parameter;});if(found==result.end())continue;
        const double first=written.points.front().time,last=std::max(first,capture.last),step=1/engine.sampleRate();const float restore=found->evaluate(last+step,value(index,*found));
        auto merged=found->points;std::erase_if(merged,[&](const auto& p){return p.time>=std::max(0.0,first-step)&&p.time<=last+step;});
        if(first>=step&&!found->points.empty())merged.push_back({first-step,found->evaluate(first-step,value(index,*found))});
        merged.insert(merged.end(),written.points.begin(),written.points.end());if(last>written.points.back().time)merged.push_back({last,written.points.back().value});if(last+step<=86400)merged.push_back({last+step,restore});
        std::sort(merged.begin(),merged.end(),[](const auto& a,const auto& b){return a.time<b.time;});
        if(merged.size()>4096){++overflowCount;continue;}found->points=std::move(merged);changed=true;
    }
    if(changed){remember({index,capture.before,result,capture.mode,t.automationMode});t.automation=std::move(result);}else t.automation=capture.before;
    for(const auto& lane:capture.pending)recordedPoints-=std::min(recordedPoints,lane.points.size());capture={};t.automationOverrideMask=0;
}
void AutomationModel::finishAll(){for(int i=0;i<engine.trackCount();++i)finish(i);}
bool AutomationModel::undo(){if(engine.isPlaying()||past.empty())return false;finishAll();AudioEngine::MutationGuard guard(engine);auto change=std::move(past.back());past.pop_back();engine.track(change.track).automation=change.before;engine.track(change.track).automationMode=change.oldMode;future.push_back(std::move(change));return true;}
bool AutomationModel::redo(){if(engine.isPlaying()||future.empty())return false;AudioEngine::MutationGuard guard(engine);auto change=std::move(future.back());future.pop_back();engine.track(change.track).automation=change.after;engine.track(change.track).automationMode=change.newMode;past.push_back(std::move(change));return true;}
}
