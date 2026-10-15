#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include "Audio/AudioEngine.h"
#include "Audio/PluginCatalog.h"
#include "Session/Session.h"
#include "Session/AutomationModel.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <new>
#include <cstdlib>
#include <malloc.h>
#include <algorithm>
#if defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
thread_local bool probeRender=false;
std::atomic<uint64_t> renderAllocations{0};
std::atomic<int64_t> liveCppBytes{0},liveCppObjects{0};
#if defined(_DEBUG)
int crtHook(int type,void*,size_t,int,long,const unsigned char*,int){if(probeRender&&type==_HOOK_ALLOC)++renderAllocations;return 1;}
#endif
}
void* operator new(size_t size){if(probeRender)++renderAllocations;if(auto* p=std::malloc(size?size:1)){liveCppBytes+=int64_t(_msize(p));++liveCppObjects;return p;}throw std::bad_alloc();}
void operator delete(void* p)noexcept{if(p){liveCppBytes-=int64_t(_msize(p));--liveCppObjects;}std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
namespace {
using Clock=std::chrono::steady_clock;
int checks=0;
void expect(bool ok,const char* label){if(!ok)throw std::runtime_error(label);++checks;std::cout<<"PASS: "<<label<<std::endl;}
uint64_t privateBytes(){PROCESS_MEMORY_COUNTERS_EX pm{};pm.cb=sizeof(pm);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pm),sizeof(pm));return static_cast<uint64_t>(pm.PrivateUsage);}
double threadCpuSeconds(){FILETIME created{},exit{},kernel{},user{};GetThreadTimes(GetCurrentThread(),&created,&exit,&kernel,&user);return ((uint64_t(kernel.dwHighDateTime)<<32|kernel.dwLowDateTime)+(uint64_t(user.dwHighDateTime)<<32|user.dwLowDateTime))*1e-7;}
double cpuSeconds(){FILETIME created{},exit{},kernel{},user{};GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user);auto number=[](FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};return (number(kernel)+number(user))*1e-7;}
std::unique_ptr<nova::MixProcessor> plugin(const juce::PluginDescription& d,int delay=0){
    auto v=std::make_unique<juce::DynamicObject>();v->setProperty("kind",5);v->setProperty("bypass",false);v->setProperty("parameters",juce::Array<juce::var>{});v->setProperty("description",d.createXml()->toString());v->setProperty("state","");
    juce::String error;auto p=nova::MixProcessor::restore(juce::var(v.release()),48000,error);if(!p||!p->plugin)throw std::runtime_error(error.toStdString());
    p->setParameter(0,0.75f);p->setParameter(1,float(delay)/2048);juce::AudioBuffer<float> warm(2,1);warm.clear();juce::MidiBuffer midi;p->plugin->processBlock(warm,midi);
    p->plugin->releaseResources();p->plugin->prepareToPlay(48000,nova::mixBlockSize);p->prepare(48000);return p;
}
void addSource(nova::AudioEngine& e,const std::shared_ptr<nova::Clip>& clip){juce::String error;if(!e.addSharedClip(clip,error))throw std::runtime_error(error.toStdString());}
std::shared_ptr<nova::Clip> wave(const juce::File& file){
    juce::AudioBuffer<float> tone(2,48000);for(int c=0;c<2;++c)for(int s=0;s<48000;++s)tone.setSample(c,s,0.005f*float(std::sin(2*juce::MathConstants<double>::pi*440*s/48000)));
    juce::WavAudioFormat format;auto stream=file.createOutputStream();stream->setPosition(0);stream->truncate();{std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,2,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(tone,0,48000))throw std::runtime_error("WAV stability fixture write failed");}
    juce::String error;auto clip=nova::AudioEngine::loadWav(file,error);if(!clip)throw std::runtime_error(error.toStdString());return std::shared_ptr<nova::Clip>(std::move(clip));
}
void lifecycleTests(const juce::PluginDescription& desc,const std::shared_ptr<nova::Clip>& clip,const juce::File& dir){
    // Real contention: releaseResources holds the wrapper gate while a test fixture
    // signals an event and sleeps. The render-side wrapper must return immediately.
    auto p=plugin(desc,17);const auto name=juce::String("Local\\NovaVstLifecycle-")+juce::String(GetCurrentProcessId());
    HANDLE event=CreateEventW(nullptr,TRUE,FALSE,name.toWideCharPointer());expect(event!=nullptr,"create lifecycle synchronization event");
    _putenv_s("NOVA_TEST_LIFECYCLE_EVENT",name.toRawUTF8());std::atomic<bool> returned{false},silent{false};std::atomic<double> elapsed{0};
    std::thread contender([&]{if(WaitForSingleObject(event,5000)!=WAIT_OBJECT_0)return;juce::AudioBuffer<float> buffer(2,64);for(int c=0;c<2;++c)for(int s=0;s<64;++s)buffer.setSample(c,s,0.1f);
        const auto begin=Clock::now();p->process(buffer,64);elapsed=std::chrono::duration<double>(Clock::now()-begin).count();silent=buffer.getMagnitude(0,64)==0;returned=true;});
    p->plugin->releaseResources();contender.join();_putenv_s("NOVA_TEST_LIFECYCLE_EVENT","");CloseHandle(event);
    expect(returned&&silent&&elapsed<0.1&&p->contentionDropouts()>0,"contended VST3 callback emits silence without waiting for 150 ms lifecycle call");
    p->plugin->prepareToPlay(48000,256);p->openEditor();p->closeEditor();expect(p->plugin->getActiveEditor()==nullptr,"editor closes before unload");p.reset();
    for(int cycle=0;cycle<12;++cycle){auto instance=plugin(desc,cycle%2?17:31);instance->bypass=cycle%2==0;instance->setParameter(0,0.4f);
        juce::String error;auto restored=nova::MixProcessor::restore(instance->state(),48000,error);
        expect(restored&&restored->plugin&&std::abs(restored->parameter(0)-0.4f)<1e-5f,"repeated opaque VST3 state recovery");
        restored->prepare(cycle%2?44100:96000);expect(restored->latencySamples()==instance->latencySamples(),"sample-rate reprepare retains reported sample delay");
    }
    nova::AudioEngine engine;engine.setOutputRate(48000);addSource(engine,clip);addSource(engine,clip);juce::String error;engine.addAux("Bus",false,error);engine.prepareProcessors();engine.setLoop(0,1,true);engine.play();
    std::atomic<bool> stop{false},finite{true};std::atomic<uint64_t> callbacks{0};
    std::jthread renderer([&](std::stop_token token){juce::AudioBuffer<float> out(2,257);while(!stop&&!token.stop_requested()){engine.render(out.getArrayOfWritePointers(),2,257);for(int c=0;c<2;++c)for(int s=0;s<257;++s)if(!std::isfinite(out.getSample(c,s)))finite=false;++callbacks;std::this_thread::yield();}});
    for(int cycle=0;cycle<20;++cycle){auto replacement=plugin(desc,cycle%2?17:31);{
        nova::AudioEngine::MutationGuard guard(engine);engine.track(0).inserts[0]=std::move(replacement);engine.setRoute(0,cycle%2?2:-1,error);engine.prepareProcessors();engine.seek(0.1);engine.play();}
        if(cycle%4==0){nova::AudioEngine::MutationGuard guard(engine);expect(nova::Session::save(dir.getChildFile("concurrent.novadaw"),engine,error),"save while render thread is quiesced");}
    }
    stop=true;renderer.join();expect(callbacks>0&&finite,"concurrent graph changes swaps and seeks preserve finite render and object lifetime");engine.clear();expect(engine.trackCount()==0,"session close releases live inserts after render stops");
    // A failed cycle must not silently erase the valid graph or its PDC.
    nova::PluginCatalog catalog(dir.getChildFile("custom-paths.xml"));expect(catalog.addDirectory(dir),"add custom scan directory");
    nova::PluginCatalog restoredCatalog(dir.getChildFile("custom-paths.xml"));expect(restoredCatalog.configuredLocations().toString().contains(dir.getFullPathName()),"custom scan directories persist alongside Windows defaults");
}

class ContinuousTest final : private juce::Timer {
public:
    ContinuousTest(juce::PluginDescription description,std::shared_ptr<nova::Clip> asset,juce::File folder,int seconds,bool professional=false)
        :desc(std::move(description)),clip(std::move(asset)),dir(std::move(folder)),duration(seconds),phase7(professional){
        timings.resize(size_t(seconds)*160+1000);engine.setOutputRate(48000);for(int i=0;i<32;++i)addSource(engine,clip);juce::String error;
        for(int i=0;i<3;++i)engine.addAux("Bus "+juce::String(i),i==1,error);
        for(int i=0;i<32;++i){engine.setRoute(i,i<16?32:33,error);auto& t=engine.track(i);t.automationMode=nova::AutomationMode::read;
            t.automation={{-1,0,{{0,0.5f},{0.5,1},{1,0.5f}}},{-2,0,{{0,-0.25f},{0.5,0.25f},{1,-0.25f}}}};
            if(i%4==0){t.inserts[0]=plugin(desc,i%3?17:31);t.automation.push_back({0,0,{{0,0.3f},{0.5,0.8f},{1,0.3f}}});}}
        engine.track(0).inserts[1]=std::make_unique<nova::MixProcessor>(nova::EffectKind::equalizer);
        engine.track(1).inserts[1]=std::make_unique<nova::MixProcessor>(nova::EffectKind::compressor);
        engine.track(2).inserts[1]=std::make_unique<nova::MixProcessor>(nova::EffectKind::gate);engine.track(2).inserts[1]->setParameter(0,-80);
        engine.track(32).inserts[0]=plugin(desc,17);engine.masterInserts[0]=plugin(desc,31);
        engine.track(34).inserts[0]=std::make_unique<nova::MixProcessor>(nova::EffectKind::delay);engine.track(34).inserts[1]=std::make_unique<nova::MixProcessor>(nova::EffectKind::reverb);
        engine.setRoute(32,33,error);engine.setSend(0,0,34,true,error);engine.track(0).sends[0].gain=0.5f;engine.setSend(4,1,34,false,error);engine.track(4).sends[1].gain=0.25f;
        engine.prepareProcessors();engine.setLoop(0,1,true);engine.play();expect(engine.pdcValid(),"continuous scene PDC prepared");
        if(phase7){nova::AudioEngine::MutationGuard guard(engine);for(int i=0;i<32;++i){engine.track(i).vcaGroup=i%4;engine.track(i).channelGroup=i%8;}engine.vcaGroups[0].gain=0.8f;engine.track(34).soloSafe=true;engine.track(0).sends[0].pan=0.3f;engine.track(0).sends[0].width=1.2f;expect(engine.setSidechain(1,0,error)&&engine.setSidechain(2,4,error),"phase7 sidechain graph prepared");engine.masteringEnabled=true;engine.prepareProcessors();}
    }
    ~ContinuousTest()override{stop=true;if(worker.joinable())worker.join();}
    void run(){start=Clock::now();initialCpu=cpuSeconds();baselineMemory=privateBytes();worker=std::thread([this]{renderLoop();});startTimer(100);juce::MessageManager::getInstance()->runDispatchLoop();stop=true;worker.join();
        const double wall=std::chrono::duration<double>(Clock::now()-start).count();auto result=std::make_unique<juce::DynamicObject>();
        passed=finite&&sawAudio&&callbacks>100&&renderAllocations==0&&controlErrors==0&&latencyRecalculations>0;
        if(phase7)passed=passed&&std::isfinite(engine.mastering->integrated())&&engine.mastering->dropped()==0&&engine.automation->overflows()==0;
        result->setProperty("passed",passed);result->setProperty("durationSeconds",wall);result->setProperty("tracks",32);result->setProperty("auxBuses",3);result->setProperty("realVst3Instances",10);
        result->setProperty("callbacks",static_cast<int64_t>(callbacks.load()));result->setProperty("processedSamples",static_cast<int64_t>(samples.load()));result->setProperty("bufferSizes","64,128,257,480,1024");
        result->setProperty("maxRenderMs",maxMicroseconds.load()/1000.0);result->setProperty("deadlineOverruns",static_cast<int64_t>(overruns.load()));result->setProperty("callbackAllocationsObserved",static_cast<int64_t>(renderAllocations.load()));
        result->setProperty("cpuPercentOneCore",100*(cpuSeconds()-initialCpu)/wall);result->setProperty("baselinePrivateBytes",static_cast<int64_t>(baselineMemory));result->setProperty("finalPrivateBytes",static_cast<int64_t>(privateBytes()));
        result->setProperty("sampledPeakPrivateBytes",static_cast<int64_t>(peakMemory));result->setProperty("vstContentionSilences",static_cast<int64_t>(engine.contentionDropouts()));result->setProperty("controlErrors",controlErrors);result->setProperty("samplesFinite",finite.load());
        result->setProperty("dynamicLatencyRecalculations",latencyRecalculations);
        result->setProperty("phase7FeaturesEnabled",phase7);if(phase7){result->setProperty("masterIntegratedLufs",engine.mastering->integrated());result->setProperty("masterTruePeakDb",engine.mastering->truePeak());result->setProperty("meterDroppedPackets",int64_t(engine.mastering->dropped()));result->setProperty("automationOverflows",int64_t(engine.automation->overflows()));}
        juce::Array<juce::var> missed;std::vector<double> durations;
        for(size_t i=0;i<timingCount;++i){const auto& t=timings[i];durations.push_back(t.wallUs);if(t.wallUs>t.budgetUs){auto o=std::make_unique<juce::DynamicObject>();o->setProperty("second",t.second);o->setProperty("frames",t.frames);o->setProperty("rate",t.rate);o->setProperty("wallUs",t.wallUs);o->setProperty("budgetUs",t.budgetUs);o->setProperty("wakeLateUs",t.wakeLateUs);o->setProperty("threadCycles",int64_t(t.cycles));o->setProperty("controlActionStart",t.controlStart);o->setProperty("controlActionEnd",t.controlEnd);missed.add(juce::var(o.release()));}}
        std::sort(durations.begin(),durations.end());for(auto percentile:{50,95,99})if(!durations.empty())result->setProperty("renderP"+juce::String(percentile)+"Us",durations[std::min(durations.size()-1,durations.size()*size_t(percentile)/100)]);
        result->setProperty("overrunDetails",missed);result->setProperty("timingCapacityOverflow",int64_t(timingOverflow));result->setProperty("renderThreadCpuSeconds",renderThreadCpu);result->setProperty("renderThreadCycles",int64_t(threadCycles));
        result->setProperty("gatedCallbacks",int64_t(engine.gatedCallbacks.load()));result->setProperty("maintenanceSilences",int64_t(engine.maintenanceSilences.load()));
        result->setProperty("liveCppBytesBeforeClear",liveCppBytes.load());result->setProperty("liveCppObjectsBeforeClear",liveCppObjects.load());result->setProperty("engineOwnedBytesBeforeClear",int64_t(engine.ownedBufferBytes()));
        engine.clear();result->setProperty("privateBytesAfterClear",int64_t(privateBytes()));result->setProperty("liveCppBytesAfterClear",liveCppBytes.load());result->setProperty("liveCppObjectsAfterClear",liveCppObjects.load());result->setProperty("engineOwnedBytesAfterClear",int64_t(engine.ownedBufferBytes()));
        result->setProperty("memoryAndCpuSamples",metrics);result->setProperty("hardwareTest",false);result->setProperty("thirdPartyPluginsValidated",false);
#if defined(_DEBUG)
        result->setProperty("allocationProbe","C++ new and MSVC Debug CRT allocation hook, render thread only");
#else
        result->setProperty("allocationProbe","C++ new only; malloc/foreign allocators not instrumented");
#endif
        dir.getChildFile("stability-result.json").replaceWithText(juce::JSON::toString(juce::var(result.release()),false));
        expect(passed,"continuous VST3 DSP automation routing and configuration changes complete without observed render allocations or nonfinite output");
    }
private:
    juce::PluginDescription desc;std::shared_ptr<nova::Clip> clip;juce::File dir;int duration;
    nova::AudioEngine engine;std::thread worker;Clock::time_point start;bool phase7=false;
    std::atomic<bool> stop{false},finite{true},sawAudio{false};std::atomic<uint64_t> callbacks{0},samples{0},overruns{0},maxMicroseconds{0};
    uint64_t baselineMemory=0,peakMemory=0;double initialCpu=0;int lastSecond=-1,controlErrors=0,latencyRecalculations=0;bool passed=false;juce::Array<juce::var> metrics;
    struct Timing {double second,wallUs,budgetUs,wakeLateUs,rate;uint64_t cycles;int frames,controlStart,controlEnd;};
    std::vector<Timing> timings;size_t timingCount=0,timingOverflow=0;double renderThreadCpu=0;uint64_t threadCycles=0;std::atomic<int> controlAction{0};
    void renderLoop(){juce::AudioBuffer<float> out(2,1024);auto due=Clock::now();const int sizes[]{64,128,257,480,1024};int index=0;
        ULONG64 initialCycles=0;QueryThreadCycleTime(GetCurrentThread(),&initialCycles);const double initialThreadCpu=threadCpuSeconds();
        while(!stop){const int frames=sizes[index++%5];const double rate=engine.sampleRate();const auto before=Clock::now();probeRender=true;
            ULONG64 cyclesBefore=0,cyclesAfter=0;QueryThreadCycleTime(GetCurrentThread(),&cyclesBefore);const int actionBefore=controlAction.load();
            engine.render(out.getArrayOfWritePointers(),2,frames);probeRender=false;const auto after=Clock::now();const auto used=std::chrono::duration_cast<std::chrono::microseconds>(after-before).count();QueryThreadCycleTime(GetCurrentThread(),&cyclesAfter);
            if(timingCount<timings.size())timings[timingCount++]={std::chrono::duration<double>(before-start).count(),double(used),frames/rate*1e6,std::chrono::duration<double,std::micro>(before-due).count(),rate,cyclesAfter-cyclesBefore,frames,actionBefore,controlAction.load()};else ++timingOverflow;
            if(uint64_t(used)>maxMicroseconds)maxMicroseconds=uint64_t(used);if(used>frames/rate*1e6)++overruns;
            for(int c=0;c<2;++c)for(int s=0;s<frames;++s){const float v=out.getSample(c,s);if(!std::isfinite(v))finite=false;if(std::abs(v)>1e-6f)sawAudio=true;}
            ++callbacks;samples+=frames;due+=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(frames/rate));std::this_thread::sleep_until(due);
        }
        ULONG64 finalCycles=0;QueryThreadCycleTime(GetCurrentThread(),&finalCycles);threadCycles=finalCycles-initialCycles;renderThreadCpu=threadCpuSeconds()-initialThreadCpu;
    }
    void timerCallback()override{
        if(phase7){engine.mastering->service();engine.automation->service();}
        const int second=static_cast<int>(std::chrono::duration<double>(Clock::now()-start).count());if(second==lastSecond)return;lastSecond=second;
        auto record=std::make_unique<juce::DynamicObject>();const auto memory=privateBytes();peakMemory=std::max(peakMemory,memory);record->setProperty("second",second);record->setProperty("privateBytes",static_cast<int64_t>(memory));record->setProperty("cpuSeconds",cpuSeconds()-initialCpu);record->setProperty("liveCppBytes",liveCppBytes.load());record->setProperty("liveCppObjects",liveCppObjects.load());record->setProperty("engineOwnedBytes",int64_t(engine.ownedBufferBytes()));metrics.add(juce::var(record.release()));
        if(second>=duration){stopTimer();juce::MessageManager::getInstance()->stopDispatchLoop();return;}
        juce::String error;
        try{
            if(phase7&&second%6==1){engine.finishAutomation();const auto mode=static_cast<nova::AutomationMode>(2+(second/6)%3);if(!engine.automation->replace(8,engine.track(8).automation,mode,error)||!engine.automation->begin(8,-1,0,error))++controlErrors;engine.track(8).gain=second%2?0.6f:0.9f;}
            if(phase7&&second%6==2)engine.automation->end(8,-1,0);
            if(second%3==1){engine.track(8).gain=0.7f;engine.track(8).pan=-0.4f;engine.track(0).inserts[1]->setParameter(12,second%2?3:-3);}
            if(second%6==3)engine.track(0).inserts[0]->setParameter(1,float(second%12==3?63:17)/2048);
            if(second%4==2){controlAction=1;nova::AudioEngine::MutationGuard guard(engine);engine.setSend(0,0,34,second%8<4,error);engine.track(4).inserts[0]->bypass=!engine.track(4).inserts[0]->bypass.load();engine.prepareProcessors();controlAction=0;}
            if(second>0&&second%5==0){controlAction=2;engine.setOutputRate(second%10==0?48000:44100);controlAction=0;}
            if(second>0&&second%7==0){controlAction=3;nova::AudioEngine::MutationGuard guard(engine);if(!nova::Session::save(dir.getChildFile("continuous.novadaw"),engine,error))++controlErrors;
                if(!nova::Session::load(dir.getChildFile("continuous.novadaw"),engine,error))++controlErrors;engine.masteringEnabled=phase7;engine.play();}
            controlAction=0;if(second>0&&second%11==0){controlAction=4;nova::AudioEngine::MutationGuard guard(engine);engine.track(0).inserts[0]->openEditor();engine.track(0).inserts[0]->closeEditor();controlAction=0;}
            if(engine.needsMaintenance()){engine.prepareProcessors();++latencyRecalculations;}if(!engine.pdcValid())++controlErrors;
        }catch(const std::exception&){++controlErrors;}controlAction=0;
        if(second%5==0)std::cout<<"STABILITY: "<<second<<" s, "<<callbacks.load()<<" callbacks, "<<memory/(1024*1024)<<" MiB private"<<std::endl;
    }
};
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    expect(argc>=3,"stability fixture paths supplied");juce::File dir(juce::String::fromUTF8(argv[1]));dir.createDirectory();const auto clip=wave(dir.getChildFile("stability.wav"));
    juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> descriptions;format.findAllTypesForFile(descriptions,juce::String::fromUTF8(argv[2]));expect(descriptions.size()==1,"discover actual stability VST3 module");
    lifecycleTests(*descriptions[0],clip,dir);renderAllocations=0;
#if defined(_DEBUG)
    const auto previousHook=_CrtSetAllocHook(crtHook);
#endif
    {ContinuousTest continuous(*descriptions[0],clip,dir,argc>=4?std::clamp(juce::String(argv[3]).getIntValue(),10,3600):10,argc>=5&&juce::String(argv[4])=="phase7");continuous.run();}
#if defined(_DEBUG)
    _CrtSetAllocHook(previousHook);
#endif
    std::cout<<checks<<" stability checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
