#include "Audio/AudioEngine.h"
#include "Audio/PluginCatalog.h"
#include "Session/Session.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <cstdlib>
#include <new>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
thread_local bool measuring=false;
thread_local int allocations=0;
#if defined(_MSC_VER) && defined(_DEBUG)
int allocationHook(int type,void*,size_t,int,long,const unsigned char*,int){if(measuring && type==_HOOK_ALLOC)++allocations;return 1;}
#endif
}
// Count application-side allocations while rendering the native signal path.
void* operator new(std::size_t size){if(measuring)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
int checks=0;
void expect(bool ok,const char* label){if(!ok)throw std::runtime_error(label);++checks;std::cout<<"PASS: "<<label<<'\n';}
bool near(float a,float b,float eps=0.0001f){return std::abs(a-b)<eps;}
juce::AudioBuffer<float> constant(int frames,float value){juce::AudioBuffer<float> b(2,frames);for(int c=0;c<2;++c)for(int s=0;s<frames;++s)b.setSample(c,s,value);return b;}
void process(nova::MixProcessor& p,juce::AudioBuffer<float>& b){for(int start=0;start<b.getNumSamples();start+=nova::mixBlockSize){const int size=std::min(nova::mixBlockSize,b.getNumSamples()-start);float* ptr[]{b.getWritePointer(0)+start,b.getWritePointer(1)+start};juce::AudioBuffer<float> view(ptr,2,size);p.process(view,size);}}
juce::AudioBuffer<float> render(nova::AudioEngine& e,int size=256){juce::AudioBuffer<float> b(2,size);e.render(b.getArrayOfWritePointers(),2,size);return b;}
juce::var vstState(const juce::PluginDescription& description){auto o=std::make_unique<juce::DynamicObject>();o->setProperty("kind",5);o->setProperty("bypass",false);o->setProperty("parameters",juce::Array<juce::var>{});o->setProperty("description",description.createXml()->toString());o->setProperty("state","");return juce::var(o.release());}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    expect(argc==4,"fixture, VST3 module and scanner supplied");juce::String error;const juce::File dir(juce::String::fromUTF8(argv[1]));dir.createDirectory();
    nova::MixProcessor comp(nova::EffectKind::compressor);comp.setParameter(0,-20);comp.setParameter(1,4);comp.setParameter(2,0.1f);comp.setParameter(4,0);comp.prepare(48000);
    auto signal=constant(48000,0.5f);process(comp,signal);
    const float expected=juce::Decibels::decibelsToGain(-20.0f+(juce::Decibels::gainToDecibels(0.5f)+20)/4);
    expect(near(signal.getSample(0,47999),expected,0.0005f) && comp.reduction>10,"compressor steady state matches threshold/ratio equation");
    nova::MixProcessor gate(nova::EffectKind::gate);gate.prepare(48000);signal=constant(4096,0.001f);process(gate,signal);expect(signal.getMagnitude(0,4096)==0,"gate suppresses below-threshold signal");
    signal=constant(4096,0.5f);process(gate,signal);expect(signal.getSample(0,4095)>0.49f,"gate opens with attack");
    signal=constant(48000,0.001f);process(gate,signal);expect(signal.getSample(0,100)>0.0009f && signal.getSample(0,47999)<0.000001f,"gate holds then releases");
    nova::MixProcessor delay(nova::EffectKind::delay);delay.setParameter(0,10);delay.setParameter(1,0.5f);delay.setParameter(2,1);delay.prepare(48000);
    signal=constant(1500,0);signal.setSample(0,0,1);process(delay,signal);
    expect(near(signal.getSample(0,0),0) && near(signal.getSample(0,480),1) && near(signal.getSample(0,960),0.5f),"delay impulse time and feedback are exact");
    delay.reset(false);signal=constant(1500,0);process(delay,signal);expect(signal.getMagnitude(0,1500)==0,"seek invalidates previous delay cells without stale echoes");
    delay.bypass=true;signal=constant(2048,0.25f);process(delay,signal);expect(near(signal.getSample(0,2047),0.25f),"native bypass returns dry signal after smoothing");
    nova::MixProcessor reverb(nova::EffectKind::reverb);reverb.setParameter(2,1);reverb.prepare(48000);signal=constant(48000,0);signal.setSample(0,0,0.5f);process(reverb,signal);
    expect(signal.getMagnitude(0,1000,47000)>0.01f && std::isfinite(signal.getRMSLevel(0,0,48000)),"JUCE reverb produces a real finite impulse tail");
    nova::MixProcessor eq(nova::EffectKind::equalizer);eq.setParameter(12,6);eq.prepare(48000);
    signal=constant(48000,0);for(int c=0;c<2;++c)for(int s=0;s<48000;++s)signal.setSample(c,s,0.1f*float(std::sin(2*juce::MathConstants<double>::pi*1000*s/48000)));
    process(eq,signal);expect(near(signal.getRMSLevel(0,40000,8000),0.1f*std::sqrt(0.5f)*juce::Decibels::decibelsToGain(6.0f),0.001f),"parametric EQ boosts sine at center frequency by 6 dB");
    expect(std::abs(eq.magnitude(1000,48000)-juce::Decibels::decibelsToGain(6.0f))<0.001,"EQ response visualization uses matching coefficients");
    for(auto kind:{nova::EffectKind::equalizer,nova::EffectKind::compressor,nova::EffectKind::gate,nova::EffectKind::delay,nova::EffectKind::reverb}){
        nova::MixProcessor p(kind);p.prepare(48000);auto restored=nova::MixProcessor::restore(p.state(),48000,error);expect(restored && restored->kind==kind,"native effect state roundtrip");
    }
    juce::ChildProcess child;const auto scanned=dir.getChildFile("scanned.xml");
    expect(child.start(juce::StringArray{juce::String::fromUTF8(argv[3]),juce::String::fromUTF8(argv[2]),scanned.getFullPathName()},0),"spawn isolated VST3 scanner");
    expect(child.waitForProcessToFinish(20000) && child.getExitCode()==0,"scanner child completes successfully");
    auto scannedXml=juce::parseXML(scanned);expect(scannedXml!=nullptr,"scanner writes real plugin descriptions");juce::KnownPluginList known;known.recreateFromXml(*scannedXml);
    expect(known.getNumTypes()==1 && known.getTypes()[0].name=="NOVA Test Gain","scanner discovers locally compiled VST3 fixture");
    auto plugin=nova::MixProcessor::restore(vstState(known.getTypes()[0]),48000,error);expect(plugin && plugin->plugin,"instantiate real VST3 through JUCE");
    signal=constant(512,0.2f);process(*plugin,signal);expect(near(signal.getSample(0,400),0.1f),"VST3 processBlock produces expected gain");
    plugin->setParameter(0,0.25f);signal=constant(512,0.2f);process(*plugin,signal);expect(near(signal.getSample(1,400),0.05f),"VST3 parameter changes affect audio");
    auto vstRestored=nova::MixProcessor::restore(plugin->state(),48000,error);expect(vstRestored&&vstRestored->plugin&&near(vstRestored->parameter(0),0.25f),"VST3 opaque state restores parameters");
    plugin->openEditor();plugin->closeEditor();expect(!plugin->plugin->getActiveEditor(),"VST3 editor opens and closes safely");
    plugin->bypass=true;signal=constant(512,0.2f);process(*plugin,signal);expect(near(signal.getSample(0,511),0.2f),"VST3 bypass yields dry audio");
    nova::AudioEngine e;e.setOutputRate(48000);e.setSmoothingTime(0);
    const auto wav=dir.getChildFile("phase3-source.wav");{juce::WavAudioFormat format;auto stream=wav.createOutputStream();stream->setPosition(0);stream->truncate();std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,2,24,{},0));auto audio=constant(48000,0.1f);expect(writer->writeFromAudioSampleBuffer(audio,0,48000),"write mathematical source fixture");}
    expect(e.addClip(nova::AudioEngine::loadWav(wav,error),error),"import source fixture");expect(e.addAux("FX",false,error),"create session return");
    e.track(0).inserts[0]=std::move(vstRestored);e.track(0).automationMode=nova::AutomationMode::read;
    e.track(0).automation={{-1,0,{{0,0},{0.1,1}}},{-2,0,{{0,-1},{0.1,1}}},{0,0,{{0,0.25f},{0.1,0.75f}}}};
    e.play();auto output=render(e,4801);
    expect(near(output.getSample(0,0),0) && near(output.getSample(1,4800),0.075f,0.001f),"sample-synchronized volume pan and VST3 parameter Read automation");
    e.seek(0.05);output=render(e);expect(near(output.getSample(0,0),0.025f,0.001f),"automation interpolation follows transport seek");
    expect(e.setSend(0,0,1,true,error),"session pre-fader send");e.track(0).sends[0].gain=0.5f;e.track(0).inputGain=1.5f;e.track(1).inserts[1]=std::make_unique<nova::MixProcessor>(nova::EffectKind::reverb);
    e.masterInserts[0]=std::make_unique<nova::MixProcessor>(nova::EffectKind::compressor);e.masterGain=0.8f;e.masterMute=true;
    const auto session=dir.getChildFile("phase3.novadaw");e.pause();expect(nova::Session::save(session,e,error),"save v3 session with plugins buses and automation");
    nova::AudioEngine loaded;loaded.setOutputRate(48000);expect(nova::Session::load(session,loaded,error),"load complete v3 session");
    expect(loaded.trackCount()==2 && loaded.track(1).auxiliary && loaded.track(0).sends[0].target==1 && loaded.track(0).sends[0].preFader
        && near(loaded.track(0).sends[0].gain,0.5f) && loaded.track(0).automation.size()==3 && loaded.track(0).inserts[0]->plugin
        && loaded.masterInserts[0] && loaded.masterMute && near(loaded.masterGain,0.8f),"v3 retains actual mixer graph plugin state automation and Master");
    auto data=juce::JSON::parse(session.loadFileAsString());auto& track=data["tracks"].getArray()->getReference(0);
    auto desc=known.getTypes()[0];desc.fileOrIdentifier=dir.getChildFile("missing.vst3").getFullPathName();track["mix"]["inserts"].getArray()->getReference(0).getDynamicObject()->setProperty("description",desc.createXml()->toString());
    const auto missing=dir.getChildFile("phase3-missing-plugin.novadaw");missing.replaceWithText(juce::JSON::toString(data));
    expect(nova::Session::load(missing,loaded,error) && loaded.track(0).inserts[0] && !loaded.track(0).inserts[0]->plugin,"missing plugin keeps slot state and opens session");
    loaded.masterMute=false;loaded.play();output=render(loaded);expect(std::isfinite(output.getMagnitude(0,256)),"missing VST3 remains safe during playback");
    expect(nova::Session::save(dir.getChildFile("phase3-missing-roundtrip.novadaw"),loaded,error),"preserve missing plugin opaque state when saving");
    data["tracks"].getArray()->getReference(1)["mix"].getDynamicObject()->setProperty("outputTarget",0);const auto invalid=dir.getChildFile("phase3-invalid.novadaw");invalid.replaceWithText(juce::JSON::toString(data));
    expect(!nova::Session::load(invalid,loaded,error) && loaded.trackCount()==2,"invalid graph leaves existing session intact");
    // Genuine v2 migration fixture: remove fields introduced in v3.
    data=juce::JSON::parse(session.loadFileAsString());data.getDynamicObject()->setProperty("version",2);data["tracks"].getArray()->remove(1);
    const auto v2=dir.getChildFile("phase2-compatible.novadaw");v2.replaceWithText(juce::JSON::toString(data));
    expect(nova::Session::load(v2,loaded,error) && !loaded.track(0).inserts[0] && loaded.track(0).outputTarget==-1,"version 2 migrates with default phase3 state");
    loaded.track(0).inserts[0]=std::make_unique<nova::MixProcessor>(nova::EffectKind::equalizer);loaded.track(0).inserts[1]=std::make_unique<nova::MixProcessor>(nova::EffectKind::delay);loaded.prepareProcessors();loaded.play();
    output=constant(4096,0);allocations=0;
#if defined(_MSC_VER) && defined(_DEBUG)
    const auto oldHook=_CrtSetAllocHook(allocationHook);
#endif
    measuring=true;loaded.render(output.getArrayOfWritePointers(),2,4096);measuring=false;
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetAllocHook(oldHook);
#endif
    expect(allocations==0,"native callback has no observed allocations (C++ new, plus Debug CRT hook)");
    loaded.track(0).automationMode=nova::AutomationMode::read;loaded.track(0).automation={{0,12,{{0,0},{0.1,6}}}};loaded.seek(0.05);output=render(loaded);
    expect(loaded.track(0).inserts[0]->parameter(12)>3 && loaded.track(0).inserts[0]->parameter(12)<3.4f,"native parameter automation interpolates in plugin units");
    // Run the same Timer-driven catalog used by the application through failure and recovery.
    {
        nova::PluginCatalog catalog(dir.getChildFile("catalog-test.xml"));catalog.known.clear();
        const juce::File module(juce::String::fromUTF8(argv[2]));const auto bundle=module.getParentDirectory().getParentDirectory().getParentDirectory();
        const juce::FileSearchPath search(bundle.getParentDirectory().getFullPathName());
        bool failureIsolated=false,recovered=false;int stage=0;
        struct Deadline final:juce::Timer { void timerCallback()override{juce::MessageManager::getInstance()->stopDispatchLoop();} } deadline;
        catalog.onChanged=[&]{if(catalog.scanning())return;if(stage++==0){failureIsolated=catalog.known.getNumTypes()==0&&!catalog.known.getBlacklistedFiles().isEmpty();
                _putenv_s("NOVA_TEST_SCAN_FAILURE","");catalog.scan(search);
            }else{recovered=catalog.known.getNumTypes()==1&&catalog.known.getBlacklistedFiles().isEmpty();juce::MessageManager::getInstance()->stopDispatchLoop();}};
        _putenv_s("NOVA_TEST_SCAN_FAILURE","1");catalog.scan(search);deadline.startTimer(45000);juce::MessageManager::getInstance()->runDispatchLoop();deadline.stopTimer();_putenv_s("NOVA_TEST_SCAN_FAILURE","");
        expect(failureIsolated,"scanner child failure leaves host alive and records blacklist");expect(recovered,"scanner retry recovers failed module and persists plugin list");
    }
    std::cout<<checks<<" Phase 3 checks passed\n";return 0;
}catch(const std::exception& ex){measuring=false;std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}}
