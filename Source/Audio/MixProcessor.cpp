#include "MixProcessor.h"
#include "Midi/MidiData.h"
#include <cmath>

namespace nova {
namespace {
const std::array<ParameterSpec, 19> eq{{
    {"Low cut Hz",20,2000,20},{"High cut Hz",1000,20000,20000},
    {"Low shelf Hz",30,2000,120},{"Low shelf dB",-18,18,0},{"Low shelf Q",0.2f,4,0.707f},
    {"High shelf Hz",1000,18000,8000},{"High shelf dB",-18,18,0},{"High shelf Q",0.2f,4,0.707f},
    {"Band 1 Hz",30,20000,250},{"Band 1 dB",-18,18,0},{"Band 1 Q",0.2f,12,1},
    {"Band 2 Hz",30,20000,1000},{"Band 2 dB",-18,18,0},{"Band 2 Q",0.2f,12,1},
    {"Band 3 Hz",30,20000,4000},{"Band 3 dB",-18,18,0},{"Band 3 Q",0.2f,12,1},
    {"Low cut on",0,1,0},{"High cut on",0,1,0}}};
const std::array<ParameterSpec,7> comp{{{"Threshold dB",-60,0,-18},{"Ratio",1,20,4},{"Attack ms",0.1f,200,10},
    {"Release ms",5,2000,100},{"Knee dB",0,24,6},{"Makeup dB",0,24,0},{"Mix",0,1,1}}};
const std::array<ParameterSpec,4> gate{{{"Threshold dB",-90,0,-45},{"Attack ms",0.1f,200,5},{"Hold ms",0,1000,50},{"Release ms",1,2000,100}}};
const std::array<ParameterSpec,3> delay{{{"Time ms",1,2000,250},{"Feedback",0,0.95f,0.3f},{"Dry/Wet",0,1,0.25f}}};
const std::array<ParameterSpec,3> reverb{{{"Room size",0,1,0.5f},{"Damping",0,1,0.5f},{"Dry/Wet",0,1,0.25f}}};
float db(float x) { return juce::Decibels::gainToDecibels(std::max(x,1.0e-9f),-180.0f); }
struct EditorWindow final : juce::DocumentWindow {
    explicit EditorWindow(juce::AudioProcessorEditor* editor, const juce::String& title)
        : DocumentWindow(title,juce::Colour(0xff1b222e),closeButton) {
        setUsingNativeTitleBar(true);setContentOwned(editor,true);setResizable(editor->isResizable(),false);
        centreWithSize(editor->getWidth(),editor->getHeight());setVisible(true);
    }
    void closeButtonPressed() override { setVisible(false); }
};
}
MixProcessor::MixProcessor(EffectKind k):kind(k) {
    for(int i=0;i<parameterCount() && i<24;++i)parameters[static_cast<size_t>(i)]=spec(i).initial;
}
MixProcessor::~MixProcessor(){closeEditor();if(plugin)plugin->releaseResources();}
int MixProcessor::parameterCount() const {
    switch(kind){case EffectKind::equalizer:return 19;case EffectKind::compressor:return 7;case EffectKind::gate:return 4;
    case EffectKind::delay:case EffectKind::reverb:return 3;case EffectKind::vst3:return plugin ? plugin->getParameters().size():0;}return 0;
}
ParameterSpec MixProcessor::spec(int i) const {
    switch(kind){case EffectKind::equalizer:return eq.at(i);case EffectKind::compressor:return comp.at(i);
    case EffectKind::gate:return gate.at(i);case EffectKind::delay:return delay.at(i);case EffectKind::reverb:return reverb.at(i);
    case EffectKind::vst3:return {"Normalized",0,1,0};}return {"",0,1,0};
}
float MixProcessor::parameter(int i)const {
    if(i<0 || i>=parameterCount())return 0;
    return kind==EffectKind::vst3 ? plugin->getParameters()[i]->getValue():parameters[static_cast<size_t>(i)].load();
}
void MixProcessor::setParameter(int i,float v){
    if(i<0 || i>=parameterCount() || !std::isfinite(v))return;
    const auto s=spec(i);v=std::clamp(v,s.minimum,s.maximum);
    if(kind==EffectKind::vst3)plugin->getParameters()[i]->setValue(v);else parameters[static_cast<size_t>(i)]=v;
}
juce::String MixProcessor::name()const {
    switch(kind){case EffectKind::equalizer:return "NOVA EQ";case EffectKind::compressor:return "NOVA Compressor";
    case EffectKind::gate:return "NOVA Gate";case EffectKind::delay:return "NOVA Delay";case EffectKind::reverb:return "NOVA Reverb";
    case EffectKind::vst3:return description.name+(faulted?" [falha]":plugin ? "":" [ausente]");}return {};
}
void MixProcessor::prepare(double rate){
    if(prepared && sampleRate==rate && !latencyChanged())return;
    prepared=false;sampleRate=rate;
    for(int i=0;i<std::min(parameterCount(),24);++i){smoothed[i].reset(rate,0.02);smoothed[i].setCurrentAndTargetValue(parameter(i));}
    wet.reset(rate,0.005);wet.setCurrentAndTargetValue(bypass.load() || faulted.load() || (kind==EffectKind::vst3 && !plugin) ? 0.0f:1.0f);
    if(kind==EffectKind::delay){delayBuffer.setSize(2,static_cast<int>(rate*2)+mixBlockSize+2);delayBuffer.clear();delayGeneration.assign(static_cast<size_t>(delayBuffer.getNumSamples()),0);currentDelayGeneration=1;}
    reverberator.setSampleRate(rate);
    if(plugin){plugin->releaseResources();plugin->setRateAndBufferSizeDetails(rate,mixBlockSize);plugin->prepareToPlay(rate,mixBlockSize);}
    midi.ensureSize(8192);
    const int reported=plugin?plugin->getLatencySamples():missingLatency;
    if(reported<0 || reported>maximumInsertLatency){failure="Latencia do insert excede 65536 amostras.";faulted=true;bypass=true;preparedLatency=0;}
    else preparedLatency=reported;
    dryCompensation.prepare(preparedLatency);reset();prepared=true;
}
void MixProcessor::reset(bool resetPlugin){filterState={};envelope=gateGain=0;holdSamples=delayWrite=0;
    // Invalidate delay cells in O(1) on seek; do not clear seconds of memory in the callback.
    if(++currentDelayGeneration==0){std::fill(delayGeneration.begin(),delayGeneration.end(),0);currentDelayGeneration=1;}
    dryCompensation.reset();reverberator.reset();if(plugin && resetPlugin)plugin->reset();}
namespace {
std::array<std::array<float,6>,7> eqCoefficients(const std::array<float,24>& p,double rate){
    std::array<std::array<float,6>,7> coefficients{};
    using C=juce::dsp::IIR::ArrayCoefficients<float>;
    auto f=[rate](float value){return std::clamp(value,1.0f,static_cast<float>(rate*0.45));};
    coefficients[0]=p[17]>=0.5f ? C::makeHighPass(rate,f(p[0]),0.707f):std::array<float,6>{1,0,0,1,0,0};
    coefficients[1]=p[18]>=0.5f ? C::makeLowPass(rate,f(p[1]),0.707f):std::array<float,6>{1,0,0,1,0,0};
    coefficients[2]=C::makeLowShelf(rate,f(p[2]),p[4],juce::Decibels::decibelsToGain(p[3]));
    coefficients[3]=C::makeHighShelf(rate,f(p[5]),p[7],juce::Decibels::decibelsToGain(p[6]));
    for(int band=0;band<3;++band){const int i=8+band*3;coefficients[4+band]=C::makePeakFilter(rate,f(p[i]),p[i+2],juce::Decibels::decibelsToGain(p[i+1]));}
    return coefficients;
}
}
void MixProcessor::updateEq(const std::array<float,24>& p,double rate){coefficients=eqCoefficients(p,rate);}
void MixProcessor::process(juce::AudioBuffer<float>& buffer,int frames,const juce::MidiBuffer* inputMidi,int midiOffset)noexcept {
    if(frames<1 || frames>mixBlockSize || buffer.getNumChannels()<2 || buffer.getNumSamples()<frames)return;
    auto* l=buffer.getWritePointer(0);auto* r=buffer.getWritePointer(1);
    for(int s=0;s<frames;++s){const auto delayed=dryCompensation.process(l[s],r[s]);dry[0][s]=delayed[0];dry[1][s]=delayed[1];}
    wet.setTargetValue(bypass.load() || faulted.load() || (kind==EffectKind::vst3 && !plugin) ? 0.0f:1.0f);
    if(kind==EffectKind::vst3){
        const bool suspended=plugin && plugin->isSuspended();
        if(suspended)wet.setCurrentAndTargetValue(0.0f);
        if(plugin && !faulted.load() && !suspended){
            // A view into preallocated stereo memory; two channels fit JUCE's inline pointer array.
            float* pointers[]{l,r};juce::AudioBuffer<float> view(pointers,2,frames);midi.clear();
            if(inputMidi){int added=0;for(const auto event:*inputMidi){
                if(event.samplePosition<midiOffset||event.samplePosition>=midiOffset+frames)continue;
                if(event.numBytes>3||event.numBytes<1)continue;
                if(++added>maximumMidiEventsPerBlock){midi.clear();midiPanic(midi);break;}
                midi.addEvent(event.data,event.numBytes,event.samplePosition-midiOffset);
            }}
            try{plugin->processBlock(view,midi);}catch(...){bypass=true;faulted=true;wet.setCurrentAndTargetValue(0);}
        }
    }else {
        for(int i=0;i<parameterCount();++i)smoothed[i].setTargetValue(parameters[i].load(std::memory_order_relaxed));
        std::array<float,24> p{};
        for(int s=0;s<frames;++s){
            for(int i=0;i<parameterCount();++i)p[i]=smoothed[i].getNextValue();
            if(kind==EffectKind::equalizer){
                if((s%16)==0)updateEq(p,sampleRate);
                for(int c=0;c<2;++c){float value=buffer.getSample(c,s);
                    for(int b=0;b<7;++b){const auto& a=coefficients[b];auto& z=filterState[c][b];
                        const float y=a[0]/a[3]*value+z[0];z[0]=a[1]/a[3]*value-a[4]/a[3]*y+z[1];z[1]=a[2]/a[3]*value-a[5]/a[3]*y;value=y;}
                    buffer.setSample(c,s,value);
                }
            }else if(kind==EffectKind::compressor){
                const float level=std::max(std::abs(l[s]),std::abs(r[s]));
                const float coefficient=std::exp(-1.0f/static_cast<float>(sampleRate*0.001*(level>envelope?p[2]:p[3])));
                envelope=level+(envelope-level)*coefficient;
                const float over=db(envelope)-p[0],knee=p[4];float gr=0;
                if(knee>0 && over>-knee/2 && over<knee/2)gr=(1-1/p[1])*(over+knee/2)*(over+knee/2)/(2*knee);
                else if(over>0)gr=(1-1/p[1])*over;
                const float gain=juce::Decibels::decibelsToGain(p[5]-gr);reduction=gr;
                l[s]*=1+p[6]*(gain-1);r[s]*=1+p[6]*(gain-1);
            }else if(kind==EffectKind::gate){
                const bool open=db(std::max(std::abs(l[s]),std::abs(r[s])))>=p[0];
                if(open)holdSamples=static_cast<int>(sampleRate*p[2]*0.001);else if(holdSamples>0)--holdSamples;
                const float target=open||holdSamples>0?1.0f:0.0f;
                const float c=std::exp(-1.0f/static_cast<float>(sampleRate*0.001*(target>gateGain?p[1]:p[3])));
                gateGain=target+(gateGain-target)*c;l[s]*=gateGain;r[s]*=gateGain;
            }else if(kind==EffectKind::delay){
                const int size=delayBuffer.getNumSamples();const float delaySamples=static_cast<float>(sampleRate*p[0]*0.001);
                float read=static_cast<float>(delayWrite)-delaySamples;while(read<0)read+=size;
                const int a=static_cast<int>(read)%size,b=(a+1)%size;const float fraction=read-std::floor(read);
                for(int c=0;c<2;++c){auto* ring=delayBuffer.getWritePointer(c);const float first=delayGeneration[a]==currentDelayGeneration?ring[a]:0,second=delayGeneration[b]==currentDelayGeneration?ring[b]:0;const float delayed=first+fraction*(second-first);
                    const float x=buffer.getSample(c,s);ring[delayWrite]=x+delayed*p[1];buffer.setSample(c,s,x*(1-p[2])+delayed*p[2]);}
                delayGeneration[delayWrite]=currentDelayGeneration;delayWrite=(delayWrite+1)%size;
            }else if(kind==EffectKind::reverb){
                juce::Reverb::Parameters rp;rp.roomSize=p[0];rp.damping=p[1];rp.wetLevel=p[2];rp.dryLevel=1-p[2];
                reverberator.setParameters(rp);reverberator.processStereo(l+s,r+s,1);
            }
        }
    }
    for(int s=0;s<frames;++s){const float w=wet.getNextValue();for(int c=0;c<2;++c){
        const float value=buffer.getSample(c,s);buffer.setSample(c,s,std::isfinite(value)?dry[c][s]+w*(value-dry[c][s]):0.0f);
    }}
}
double MixProcessor::magnitude(double frequency,double rate)const {
    if(kind!=EffectKind::equalizer || bypass)return 1;
    std::array<float,24> p{};for(int i=0;i<19;++i)p[i]=parameter(i);
    // Reuse JUCE's coefficient formulas without mutating the audio-owned filters.
    const auto response=eqCoefficients(p,rate);double result=1;
    const double angle=2*juce::MathConstants<double>::pi*frequency/rate;
    const std::complex<double> z=std::polar(1.0,-angle);
    for(const auto& a:response)result*=std::abs((double(a[0])+double(a[1])*z+double(a[2])*z*z)/(double(a[3])+double(a[4])*z+double(a[5])*z*z));
    return result;
}
juce::var MixProcessor::state()const {
    auto o=std::make_unique<juce::DynamicObject>();o->setProperty("kind",static_cast<int>(kind));o->setProperty("bypass",bypass.load());
    juce::Array<juce::var> values;for(int i=0;i<parameterCount() && kind!=EffectKind::vst3;++i)values.add(parameter(i));o->setProperty("parameters",values);o->setProperty("latencySamples",preparedLatency);
    if(kind==EffectKind::vst3){auto xml=description.createXml();o->setProperty("description",xml->toString());juce::MemoryBlock memory=savedState;
        if(plugin)plugin->getStateInformation(memory);o->setProperty("state",memory.toBase64Encoding());}
    return juce::var(o.release());
}
std::unique_ptr<MixProcessor> MixProcessor::restore(const juce::var& v,double rate,juce::String& error){
    if(!v.isObject() || !v["kind"].isInt() || int(v["kind"])<0 || int(v["kind"])>5 || !v["bypass"].isBool() || !v["parameters"].isArray()){
        error="Insert invalido.";return {};}
    auto p=std::make_unique<MixProcessor>(static_cast<EffectKind>(int(v["kind"])));p->bypass=bool(v["bypass"]);
    if(!v["latencySamples"].isVoid()){
        if(!v["latencySamples"].isInt() || int(v["latencySamples"])<0 || int(v["latencySamples"])>maximumInsertLatency){error="Latencia persistida do insert invalida.";return {};}
        if(p->kind==EffectKind::vst3)p->missingLatency=int(v["latencySamples"]);
    }
    if(p->kind==EffectKind::vst3){
        auto xml=juce::parseXML(v["description"].toString());
        if(!xml || !p->description.loadFromXml(*xml) || p->description.pluginFormatName!="VST3"
            || !v["state"].isString() || (v["state"].toString().isNotEmpty() && !p->savedState.fromBase64Encoding(v["state"].toString()))){error="Estado VST3 invalido.";return {};}
        juce::AudioPluginFormatManager formats;formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
        p->plugin=formats.createPluginInstance(p->description,rate,mixBlockSize,p->failure);
        if(p->plugin){
            auto layout=p->plugin->getBusesLayout();
            const bool instrument=p->description.isInstrument;
            if(layout.outputBuses.size()!=1 || (instrument?layout.inputBuses.size()>1:layout.inputBuses.size()!=1) || (instrument&&!p->plugin->acceptsMidi())){p->failure="Efeito stereo ou instrumento MIDI com saida stereo sem sidechain exigido.";p->plugin.reset();}
            else{if(!layout.inputBuses.isEmpty())layout.inputBuses.set(0,instrument?juce::AudioChannelSet::disabled():juce::AudioChannelSet::stereo());layout.outputBuses.set(0,juce::AudioChannelSet::stereo());
                if(!p->plugin->setBusesLayout(layout)){p->failure="Efeito nao suporta stereo.";p->plugin.reset();}}
            if(p->plugin && p->savedState.getSize())p->plugin->setStateInformation(p->savedState.getData(),static_cast<int>(p->savedState.getSize()));
        }
    }else {
        auto* values=v["parameters"].getArray();if(values->size()!=p->parameterCount()){error="Numero de parametros invalido.";return {};}
        for(int i=0;i<values->size();++i){const auto& value=values->getReference(i);const float f=float(value);const auto s=p->spec(i);
            if(!(value.isDouble()||value.isInt()||value.isInt64()) || !std::isfinite(f) || f<s.minimum || f>s.maximum){error="Parametro de efeito invalido.";return {};}
            p->parameters[i]=f;
        }
    }
    p->prepare(rate);return p;
}
bool MixProcessor::latencyChanged()const noexcept{return plugin && !faulted.load() && plugin->getLatencySamples()!=preparedLatency;}
std::unique_ptr<MixProcessor> MixProcessor::loadPlugin(const juce::PluginDescription& description,double rate,juce::String& error){
    auto v=std::make_unique<juce::DynamicObject>();v->setProperty("kind",5);v->setProperty("bypass",false);v->setProperty("parameters",juce::Array<juce::var>{});
    v->setProperty("description",description.createXml()->toString());v->setProperty("state","");return restore(juce::var(v.release()),rate,error);
}
uint64_t MixProcessor::contentionDropouts()const noexcept{return plugin?plugin->getNonBlockingDropoutCount():0;}
void MixProcessor::openEditor(){if(!plugin)return;if(!editorWindow){auto* editor=plugin->createEditorIfNeeded();if(!editor)editor=new juce::GenericAudioProcessorEditor(*plugin);editorWindow=std::make_unique<EditorWindow>(editor,name());}editorWindow->setVisible(true);editorWindow->toFront(true);}
void MixProcessor::closeEditor(){editorWindow.reset();}
float AutomationLane::evaluate(double time,float fallback)const noexcept {
    if(points.empty())return fallback;if(time<=points.front().time)return points.front().value;
    auto next=std::upper_bound(points.begin(),points.end(),time,[](double t,const auto& p){return t<p.time;});
    if(next==points.end())return points.back().value;const auto& a=*(next-1);const auto& b=*next;
    return a.value+static_cast<float>((time-a.time)/(b.time-a.time))*(b.value-a.value);
}
}
