#include "Audio/AudioEngine.h"
#include "Session/AutomationModel.h"
#include "Session/Session.h"
#include <iostream>
#include <cmath>
#include <new>
#include <cstdlib>
namespace {thread_local bool probe=false;int allocations=0,checks=0;}
void* operator new(size_t n){if(probe)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,size_t)noexcept{std::free(p);}
namespace {
void expect(bool ok,const char* label){if(!ok)throw std::runtime_error(label);++checks;std::cout<<"PASS: "<<label<<'\n';}
void advance(nova::AudioEngine& engine,int frames=480){juce::AudioBuffer<float> out(2,frames);probe=true;engine.render(out.getArrayOfWritePointers(),2,frames);probe=false;engine.automation->service();}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    expect(argc==3,"automation fixture directory and real VST3 supplied");juce::File directory(juce::String::fromUTF8(argv[1]));directory.createDirectory();const auto dir=directory.getChildFile(juce::Uuid().toString());dir.createDirectory();juce::String error;
    const auto file=dir.getChildFile("source.wav");juce::AudioBuffer<float> signal(2,48000);for(int c=0;c<2;++c)for(int n=0;n<48000;++n)signal.setSample(c,n,0.2f);
    {juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(file.createOutputStream().release(),48000,2,24,{},0));expect(writer&&writer->writeFromAudioSampleBuffer(signal,0,48000),"write actual automation audio source");}
    nova::AudioEngine engine;engine.setOutputRate(48000);engine.setSmoothingTime(0);expect(engine.addClip(nova::AudioEngine::loadWav(file,error),error),"load automation source");
    auto& model=*engine.automation;const std::vector<nova::AutomationLane> baseline{{-1,0,{{0,0.25f},{0.8,0.25f}}}};
    expect(model.replace(0,baseline,nova::AutomationMode::touch,error),"configure Touch automation");engine.play();advance(engine);expect(model.begin(0,-1,0,error),"begin fader touch without mutating lane points on audio thread");engine.track(0).gain=0.8f;advance(engine);advance(engine);model.end(0,-1,0);
    expect(engine.track(0).automationOverrideMask==0&&engine.track(0).automation[0].evaluate(0.02,0)>0.79f,"Touch writes performed interval and clears override on release");
    juce::AudioBuffer<float> out(2,32);engine.render(out.getArrayOfWritePointers(),2,32);expect(std::abs(out.getSample(0,31)-0.05f)<1e-5f,"Touch returns to underlying Read curve after release");engine.pause();model.service();expect(model.undo()&&engine.track(0).automation[0].points.size()==2,"Undo restores pre-touch curve");expect(model.redo()&&engine.track(0).automation[0].points.size()>2,"Redo restores captured fader interval");
    engine.seek(0);expect(model.replace(0,baseline,nova::AutomationMode::latch,error),"configure Latch");engine.play();advance(engine);model.begin(0,-1,0,error);engine.track(0).gain=0.6f;advance(engine);model.end(0,-1,0);advance(engine);
    expect(engine.track(0).automationOverrideMask!=0,"Latch remains overridden after release");engine.render(out.getArrayOfWritePointers(),2,32);expect(std::abs(out.getSample(0,31)-0.12f)<1e-5f,"Latch keeps real audio gain until transport stop");engine.pause();model.service();expect(engine.track(0).automationOverrideMask==0&&engine.track(0).automation[0].evaluate(0.025,0)>0.59f,"Stop commits latch capture and clears held state");
    engine.seek(0);model.replace(0,baseline,nova::AutomationMode::write,error);engine.play();model.service();for(int n=0;n<5;++n){engine.track(0).gain=0.3f+n*0.1f;advance(engine);}engine.pause();model.service();expect(engine.track(0).automation[0].evaluate(0.03,0)>0.49f,"Write records existing lane without requiring a held gesture");
    expect(model.replace(0,{{-2,0,{{0,-1,1},{0.5,1,2},{1,-1}}}},nova::AutomationMode::read,error),"configure editable hold and smooth pan curves");const auto& curve=engine.track(0).automation[0];expect(curve.evaluate(0.25,0)==-1&&std::abs(curve.evaluate(0.625,0)-0.6875f)<1e-6f,"hold and smoothstep interpolate real parameter values");
    const auto session=dir.getChildFile("automation-v7.novadaw");expect(nova::Session::save(session,engine,error),"save version 7 automation curves");nova::AudioEngine loaded;loaded.setOutputRate(48000);expect(nova::Session::load(session,loaded,error)&&loaded.track(0).automation[0].points[0].curve==1,"session restores curve interpolation and mode");
    auto invalid=juce::JSON::parse(session.loadFileAsString());invalid["tracks"].getArray()->getReference(0)["mix"]["automation"].getArray()->getReference(0)["points"].getArray()->getReference(0).getDynamicObject()->setProperty("curve",9);const auto bad=dir.getChildFile("bad.novadaw");bad.replaceWithText(juce::JSON::toString(invalid));expect(!nova::Session::load(bad,loaded,error)&&loaded.trackCount()==1,"invalid automation curve rejected without replacing live session");
    juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> descriptions;format.findAllTypesForFile(descriptions,juce::String::fromUTF8(argv[2]));expect(descriptions.size()==1,"discover real original VST3 for recorded plugin parameter");
    {nova::AudioEngine::MutationGuard guard(engine);engine.track(0).inserts[0]=nova::MixProcessor::loadPlugin(*descriptions[0],48000,error);engine.prepareProcessors();}
    model.replace(0,{{0,0,{{0,0.1f},{0.8,0.1f}}}},nova::AutomationMode::touch,error);engine.seek(0);engine.play();advance(engine);model.begin(0,0,0,error);engine.track(0).inserts[0]->setParameter(0,0.9f);advance(engine);model.end(0,0,0);engine.pause();model.service();expect(engine.track(0).automation[0].evaluate(0.02,0)>0.89f,"Touch captures actual VST3 parameter changes");
    expect(model.replace(0,{{-1,0,{{0,1},{0,0}}}},nova::AutomationMode::read,error)==false,"duplicate automation times rejected transactionally");
    expect(allocations==0,"automation playback and overrides allocate no observed C++ storage on audio thread");
    std::cout<<checks<<" advanced automation checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
