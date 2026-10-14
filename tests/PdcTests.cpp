#include "Audio/AudioEngine.h"
#include "Session/Session.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
namespace {
int checks=0;
void expect(bool value,const char* label){if(!value)throw std::runtime_error(label);++checks;std::cout<<"PASS: "<<label<<'\n';}
bool near(float a,float b){return std::abs(a-b)<0.00002f;}
void source(nova::AudioEngine& e,float amplitude=0.1f){auto clip=std::make_unique<nova::Clip>();clip->sampleRate=48000;clip->samples.setSize(2,48000);clip->samples.clear();for(int c=0;c<2;++c)clip->samples.setSample(c,0,amplitude);juce::String error;expect(e.addClip(std::move(clip),error),"add impulse source");}
juce::AudioBuffer<float> render(nova::AudioEngine& e,int frames){juce::AudioBuffer<float> out(2,frames);e.render(out.getArrayOfWritePointers(),2,frames);return out;}
std::unique_ptr<nova::MixProcessor> delayedPlugin(const juce::PluginDescription& desc,int delay,float gain=1){
    auto v=std::make_unique<juce::DynamicObject>();v->setProperty("kind",5);v->setProperty("bypass",false);v->setProperty("parameters",juce::Array<juce::var>{});v->setProperty("description",desc.createXml()->toString());
    v->setProperty("state","");juce::String error;auto p=nova::MixProcessor::restore(juce::var(v.release()),48000,error);
    if(!p || !p->plugin)throw std::runtime_error(error.toStdString());
    p->setParameter(0,gain);p->setParameter(1,float(delay)/2048.0f);
    // Apply actual VST3 normalized parameters, then reprepare off-thread so the wrapper
    // re-queries the component's reported latency. Opaque host state is never fabricated.
    juce::AudioBuffer<float> warm(2,1);warm.clear();juce::MidiBuffer midi;p->plugin->processBlock(warm,midi);
    p->plugin->releaseResources();p->plugin->prepareToPlay(48000,nova::mixBlockSize);p->prepare(48000);
    expect(p && p->plugin && p->latencySamples()==delay,"real VST3 reports configured processing latency");return p;
}
void impulse(const juce::AudioBuffer<float>& out,int position,float amplitude){bool ok=true;for(int c=0;c<2;++c)for(int s=0;s<out.getNumSamples();++s)ok&=near(out.getSample(c,s),s==position?amplitude:0);expect(ok,"all paths sum at exactly one compensated impulse");}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    expect(argc==3,"PDC fixture paths supplied");juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> descriptions;format.findAllTypesForFile(descriptions,juce::String::fromUTF8(argv[2]));expect(descriptions.size()==1,"discover real VST3 PDC fixture");const auto desc=*descriptions[0];juce::String error;
    for(int delay:{0,1,17,255,256,511}){
        nova::AudioEngine e;e.setOutputRate(48000);e.setSmoothingTime(0);source(e);source(e);
        e.track(0).inserts[0]=delayedPlugin(desc,delay);e.prepareProcessors();expect(e.pdcValid()&&e.latencySamples()==delay,"parallel track latency equals longest path");e.play();impulse(render(e,delay+300),delay,0.2f);
        e.stop();e.track(0).inserts[0]->bypass=true;e.prepareProcessors();e.play();impulse(render(e,delay+300),delay,0.2f);
    }
    nova::AudioEngine buses;buses.setOutputRate(48000);buses.setSmoothingTime(0);source(buses);source(buses);
    expect(buses.addAux("A",false,error)&&buses.addAux("B",false,error),"add chained buses");
    buses.track(0).inserts[0]=delayedPlugin(desc,17);buses.track(2).inserts[0]=delayedPlugin(desc,31);buses.track(3).inserts[0]=delayedPlugin(desc,7);buses.masterInserts[0]=delayedPlugin(desc,13);
    expect(buses.setRoute(0,2,error)&&buses.setRoute(1,2,error)&&buses.setRoute(2,3,error),"connect parallel sources through two latent buses");buses.prepareProcessors();expect(buses.latencySamples()==68,"track bus and Master latencies accumulate");buses.play();impulse(render(buses,600),68,0.2f);
    for(bool pre:{false,true}){
        nova::AudioEngine sends;sends.setOutputRate(48000);sends.setSmoothingTime(0);source(sends);source(sends);
        expect(sends.addAux("Return",false,error),"add send return");sends.track(0).inserts[0]=delayedPlugin(desc,17);sends.track(2).inserts[0]=delayedPlugin(desc,23);
        sends.track(0).gain=0.5f;expect(sends.setSend(0,0,2,pre,error),"connect pre/post send");sends.track(0).sends[0].gain=1;sends.prepareProcessors();sends.play();
        impulse(render(sends,512),40,pre?0.25f:0.2f);expect(sends.latencySamples()==40,"send return path determines global latency");
    }
    // The fader sees samples emitted by the insert, so Read uses their source timestamp.
    nova::AudioEngine automation;automation.setOutputRate(48000);automation.setSmoothingTime(0);source(automation);
    automation.track(0).inserts[0]=delayedPlugin(desc,256);automation.track(0).automationMode=nova::AutomationMode::read;
    automation.track(0).automation={{-1,0,{{0,1},{256.0/48000,0}}}};automation.prepareProcessors();automation.play();impulse(render(automation,600),256,0.1f);
    // Route edits are transactional even when they invalidate a preallocated PDC plan.
    expect(!buses.setRoute(3,2,error)&&buses.latencySamples()==68&&buses.track(3).outputTarget==-1,"cycle rejection preserves validated PDC plan");
    expect(!buses.setSend(3,1,2,true,error)&&buses.latencySamples()==68,"send cycle rejection preserves latency");
    nova::DelayCompensation budget;std::vector<nova::PdcNode> invalid(2);invalid[0].latency=nova::StereoDelay::maximumDelay;invalid[0].output=1;invalid[1].auxiliary=true;invalid[1].latency=1;
    expect(!budget.build(invalid,0,error),"accumulated latency overflow rejected");
    nova::StereoDelay ring;ring.prepare(3);ring.process(1,1);ring.reset();bool silent=true;for(int i=0;i<6;++i){auto out=ring.process(0,0);silent&=out[0]==0&&out[1]==0;}expect(silent,"PDC seek reset discards queued audio without clearing buffers");
    // A missing plugin keeps its last known latency and delayed dry fallback in v3.
    const juce::File dir(juce::String::fromUTF8(argv[1]));dir.createDirectory();const auto missing=desc;auto missingDesc=missing;missingDesc.fileOrIdentifier=dir.getChildFile("absent.vst3").getFullPathName();
    auto state=delayedPlugin(desc,17)->state();state.getDynamicObject()->setProperty("description",missingDesc.createXml()->toString());auto placeholder=nova::MixProcessor::restore(state,48000,error);
    expect(placeholder&&!placeholder->plugin&&placeholder->latencySamples()==17,"missing VST3 preserves declared latency");juce::AudioBuffer<float> buffer(2,64);buffer.clear();buffer.setSample(0,0,0.1f);placeholder->process(buffer,64);expect(near(buffer.getSample(0,0),0)&&near(buffer.getSample(0,17),0.1f),"missing VST3 dry fallback is compensated");
    std::cout<<checks<<" PDC checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
