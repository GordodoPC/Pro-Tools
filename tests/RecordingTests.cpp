#include "Audio/AudioEngine.h"
#include "Audio/AudioRecorder.h"
#include "Session/Session.h"
#include <iostream>
#include <stdexcept>
#include <new>
#include <cstdlib>
#include <chrono>
namespace {thread_local bool renderProbe=false;std::atomic<int> renderAllocations{0};}
void* operator new(size_t size){if(renderProbe)++renderAllocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}
namespace {int checks=0;void expect(bool ok,const char* text){if(!ok)throw std::runtime_error(text);++checks;std::cout<<"PASS: "<<text<<'\n';}
void input(nova::AudioEngine& engine,int frames=400){juce::AudioBuffer<float> source(2,frames),out(2,frames);for(int i=0;i<frames;++i){source.setSample(0,i,0.1f);source.setSample(1,i,0.2f);}renderProbe=true;engine.renderWithInput(source.getArrayOfReadPointers(),2,out.getArrayOfWritePointers(),2,frames);renderProbe=false;}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    expect(argc==2,"recording fixture directory supplied");const juce::File dir(juce::String::fromUTF8(argv[1]));dir.createDirectory();juce::String error;
    nova::AudioEngine engine;engine.setOutputRate(48000);engine.setSmoothingTime(0);expect(engine.addAudioTrack("Input A",error)&&engine.addAudioTrack("Input B",error),"create two empty non-destructive audio tracks");
    engine.track(0).recordArm=true;engine.track(1).recordArm=true;engine.track(1).recordInputLeft=1;expect(!engine.track(0).monitorInput&&!engine.track(1).monitorInput,"input monitoring defaults off");
    nova::AudioRecorder recorder(engine);engine.attachRecorder(&recorder);expect(recorder.start(dir,2,{},error),"start simultaneous capture of distinct simulated input channels");input(engine);
    expect(recorder.stop(error)&&engine.track(0).takes.size()==1&&engine.track(1).takes.size()==1,"disk worker closes two valid WAV takes");
    expect(engine.track(0).regions[0].asset->samples.getNumSamples()==400&&engine.track(1).regions[0].asset->samples.getNumSamples()==400,"recorded mono WAVs contain exact frame count");
    expect(std::abs(engine.track(0).regions[0].asset->samples.getSample(0,0)-0.1f)<1e-6&&std::abs(engine.track(1).regions[0].asset->samples.getSample(0,0)-0.2f)<1e-6,"distinct input signal values survive actual WAV write and decode");
    engine.stop();engine.play();juce::AudioBuffer<float> playback(2,400);engine.render(playback.getArrayOfWritePointers(),2,400);expect(playback.getMagnitude(0,400)>0.2f,"captured files play through original mixer");
    engine.track(0).monitorPreference=true;engine.track(0).monitorInput=true;
    const auto session=dir.getChildFile("recorded-v5.novadaw");expect(nova::Session::save(session,engine,error),"persist recording inputs takes and regions in session");nova::AudioEngine reopened;reopened.setOutputRate(48000);
    expect(nova::Session::load(session,reopened,error)&&reopened.track(1).recordInputLeft==1&&reopened.track(0).takes.size()==1&&reopened.track(0).regions.size()==1,"reopen recorded session preserves independent channel takes");
    expect(!reopened.track(0).monitorInput&&reopened.track(0).monitorPreference,"monitor preference persists but reopened sessions require explicit monitoring opt-in");
    nova::AudioEngine punch;punch.setOutputRate(48000);punch.addAudioTrack("Punch",error);punch.track(0).recordArm=true;nova::AudioRecorder punchRecorder(punch);punch.attachRecorder(&punchRecorder);
    nova::RecordingOptions options;options.punch=true;options.punchIn=16.0/48000;options.punchOut=80.0/48000;options.preRoll=8.0/48000;options.postRoll=8.0/48000;
    expect(punchRecorder.start(dir,2,options,error),"configure punch and pre/post roll off callback");input(punch,100);expect(punchRecorder.stop(error)&&punch.track(0).regions[0].asset->samples.getNumSamples()==64,"punch captures only exact selected input samples");
    expect(std::abs(punch.track(0).regions[0].start-16.0/48000)<1e-9,"punch recording aligns region to timeline");
    nova::AudioEngine loop;loop.setOutputRate(48000);loop.addAudioTrack("Loop",error);loop.track(0).recordArm=true;nova::AudioRecorder loopRecorder(loop);loop.attachRecorder(&loopRecorder);options={};options.loop=true;options.loopEnd=64.0/48000;
    expect(loopRecorder.start(dir,2,options,error),"start loop recording with fixed prepared queues");input(loop,192);expect(loopRecorder.stop(error)&&loop.track(0).takes.size()==3,"loop boundaries create three separate preserved WAV takes");
    expect(loop.track(0).regions.size()==1&&loop.track(0).regions[0].asset->samples.getNumSamples()==64,"last loop take is active without summing alternate takes");
    const auto first=loop.track(0).takes.front().id;expect(loop.activateTake(0,first,error)&&loop.track(0).activeTake==first,"select earlier take without deleting its source files");
    nova::AudioEngine failure;failure.setOutputRate(48000);failure.addAudioTrack("Failure",error);failure.track(0).recordArm=true;nova::AudioRecorder failed(failure);failure.attachRecorder(&failed);options={};options.failAfterFrames=256;
    expect(failed.start(dir,2,options,error),"start deterministic recording writer failure fixture");input(failure,512);expect(!failed.stop(error)&&!failure.track(0).takes.empty()&&failure.track(0).regions[0].asset->samples.getNumSamples()==256,"writer failure reports interruption and preserves valid received WAV prefix");
    expect(!recorder.start(dir,1,{},error),"unavailable selected input is rejected before capture");
    nova::AudioEngine recovery;recovery.setOutputRate(48000);nova::AudioRecorder recoveryRecorder(recovery);
    expect(recoveryRecorder.recover(failed.captureDirectory(),error)&&recovery.track(0).regions[0].asset->samples.getNumSamples()==256,"recovery manifest restores valid WAV prefix without modifying source file");
    nova::AudioEngine stereo;stereo.setOutputRate(48000);stereo.setSmoothingTime(0);stereo.addAudioTrack("Stereo",error);stereo.track(0).recordArm=true;stereo.track(0).recordStereo=true;stereo.track(0).recordGain=0.5f;nova::AudioRecorder stereoRecorder(stereo);stereo.attachRecorder(&stereoRecorder);
    expect(stereoRecorder.start(dir,2,{},error),"start stereo capture with per-track digital gain");input(stereo,257);expect(stereoRecorder.stop(error)&&stereo.track(0).regions[0].asset->samples.getNumChannels()==2,"stereo WAV preserves both selected channels at odd buffer boundary");
    expect(std::abs(stereo.track(0).regions[0].asset->samples.getSample(0,0)-0.05f)<1e-6&&std::abs(stereo.track(0).regions[0].asset->samples.getSample(1,0)-0.1f)<1e-6,"software capture gain affects both channels");
    stereo.pause();stereo.track(0).monitorInput=true;juce::AudioBuffer<float> monitorInput(2,32),monitorOutput(2,32);for(int i=0;i<32;++i){monitorInput.setSample(0,i,0.1f);monitorInput.setSample(1,i,0.2f);}stereo.renderWithInput(monitorInput.getArrayOfReadPointers(),2,monitorOutput.getArrayOfWritePointers(),2,32);
    expect(monitorOutput.getMagnitude(0,32)>0.04f&&stereo.track(0).inputPeak.load()>=0.2f,"explicit input monitoring and stereo peak operate while transport is stopped");
    stereo.track(0).monitorInput=false;expect(stereoRecorder.start(dir,2,{},error),"restart capture without overwriting preserved sources");input(stereo,64);juce::AudioBuffer<float> missing(2,64);stereo.renderWithInput(nullptr,0,missing.getArrayOfWritePointers(),2,64);
    expect(!stereoRecorder.stop(error)&&stereo.track(0).takes.size()==2,"input disappearance interrupts capture with valid prefix retained");
    expect(stereoRecorder.start(dir,2,{},error),"start sample-rate interruption fixture");input(stereo,32);stereo.setOutputRate(44100);input(stereo,32);expect(!stereoRecorder.stop(error),"sample-rate change interrupts instead of silently mislabelling WAV rate");
    nova::AudioEngine concurrent;concurrent.setOutputRate(48000);concurrent.addAudioTrack("Concurrent A",error);concurrent.addAudioTrack("Concurrent B",error);concurrent.track(0).recordArm=true;concurrent.track(1).recordArm=true;concurrent.track(1).recordInputLeft=1;nova::AudioRecorder concurrentRecorder(concurrent);concurrent.attachRecorder(&concurrentRecorder);expect(concurrentRecorder.start(dir,2,{},error),"start concurrent simulated capture");
    std::atomic<int> rendered{0};std::atomic<bool> finish{false};std::thread producer([&]{while(!finish){input(concurrent,64+(rendered.load()%4)*64);++rendered;std::this_thread::sleep_for(std::chrono::milliseconds(1));}});
    for(int i=0;i<50;++i){concurrent.track(0).recordGain=0.5f+(i%2)*0.5f;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    const bool stopped=concurrentRecorder.stop(error);finish=true;producer.join();expect(stopped&&rendered>20&&concurrent.track(0).takes.size()==1&&concurrent.track(1).takes.size()==1,"control-thread stop quiesces live capture before worker flush and take adoption");
    expect(concurrentRecorder.overflowCount()==0,"paced capture with variable buffers does not overflow prepared queues");
    expect(renderAllocations==0,"capture callback performs zero observed C++ new allocations");
    const auto original=stereo.track(0).takes.front().file;expect(original.existsAsFile()&&original.getSize()>44,"later captures and input interruptions preserve original WAV");
    const auto punchSession=dir.getChildFile("punch-v5.novadaw");expect(nova::Session::save(punchSession,punch,error)&&nova::Session::load(punchSession,reopened,error)&&reopened.recordingOptions.punch&&std::abs(reopened.recordingOptions.preRoll-8.0/48000)<1e-9,"punch and pre/post roll settings persist independently of capture thread");
    auto legacy=juce::JSON::parse(session.loadFileAsString());legacy.getDynamicObject()->setProperty("version",4);const auto legacyFile=dir.getChildFile("legacy-v4.novadaw");legacyFile.replaceWithText(juce::JSON::toString(legacy));expect(nova::Session::load(legacyFile,reopened,error)&&reopened.track(0).regions.size()==1&&reopened.track(0).takes.empty(),"version 4 audio regions migrate without requiring version 5 take metadata");
    const auto invalid=dir.getChildFile("invalid-recovery");invalid.createDirectory();invalid.getChildFile("recovery.json").replaceWithText("{\"format\":\"nova-recording-recovery\",\"files\":[{\"track\":0,\"file\":\"../outside.wav\",\"start\":0}]}" );const auto before=recovery.trackCount();expect(!recoveryRecorder.recover(invalid,error)&&recovery.trackCount()==before,"recovery rejects paths outside capture folder without changing session");
    std::cout<<checks<<" recording checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
