#include "Audio/AudioEngine.h"
#include "Session/Session.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
namespace {
int checks=0;
void expect(bool ok,const char* label){if(!ok)throw std::runtime_error(label);++checks;std::cout<<"PASS: "<<label<<'\n';}
bool near(float a,float b){return std::abs(a-b)<0.0001f;}
void source(nova::AudioEngine& e,float value=0.1f){auto clip=std::make_unique<nova::Clip>();clip->samples.setSize(2,48000);clip->sampleRate=48000;
    for(int c=0;c<2;++c)for(int s=0;s<48000;++s)clip->samples.setSample(c,s,value);juce::String error;expect(e.addClip(std::move(clip),error),"add source");}
juce::AudioBuffer<float> render(nova::AudioEngine& e,int frames=256){juce::AudioBuffer<float> b(2,frames);e.render(b.getArrayOfWritePointers(),2,frames);return b;}
}
int main(){juce::ScopedJuceInitialiser_GUI juceInit;try{
    nova::AudioEngine e;e.setOutputRate(48000);e.setSmoothingTime(0);source(e);source(e,0.2f);juce::String error;
    expect(e.addAux("Return",false,error),"add stereo Aux");expect(e.setRoute(0,2,error),"route Track to Aux");
    e.track(2).gain=0.5f;e.play();auto out=render(e);expect(near(out.getSample(0,0),0.25f),"topological bus sum and fader");
    e.track(0).solo=true;out=render(e);expect(near(out.getSample(0,0),0.05f),"source solo retains downstream Aux");
    e.track(0).solo=false;e.track(2).solo=true;out=render(e);expect(near(out.getSample(0,0),0.05f),"bus solo admits upstream source");
    e.track(2).solo=false;e.track(1).mute=true;expect(e.setRoute(0,-1,error),"route source back to Master");
    e.track(0).gain=0;expect(e.setSend(0,0,2,true,error),"configure pre fader send");e.track(0).sends[0].gain=1;
    out=render(e);expect(near(out.getSample(0,0),0.05f),"pre send survives zero fader");
    expect(e.setSend(0,0,2,false,error),"configure post fader send");out=render(e);expect(out.getMagnitude(0,256)==0,"post send follows zero fader");
    expect(!e.setRoute(2,0,error) && e.track(2).outputTarget==-1,"reject route to audio source");
    expect(e.addAux("Bus B",true,error),"add mono bus");expect(e.setRoute(2,3,error),"bus to bus route");
    expect(!e.setRoute(3,2,error) && e.track(3).outputTarget==-1,"reject cycle transactionally");
    expect(!e.setSend(3,1,2,true,error) && e.track(3).sends[1].target==-1,"reject send feedback");
    expect(!e.setRoute(2,2,error),"reject self feedback");
    e.track(0).gain=1;e.track(0).inputGain=2;e.track(0).sends[0].gain=0;
    for(int size:{1,17,64,257,480,1024,4096}){out=render(e,size);expect(near(out.getSample(0,0),0.2f) && near(out.getSample(0,size-1),0.2f),"arbitrary buffer size preserves samples");}
    e.track(0).inputGain=16;out=render(e);expect(e.track(0).clipped && e.masterClipped && out.getSample(0,0)==1,"latched channel and Master clip detection");
    e.track(0).inputGain=1;e.masterMute=true;out=render(e);expect(out.getMagnitude(0,256)==0,"Master mute");
    nova::AudioEngine smooth;smooth.setOutputRate(48000);source(smooth);smooth.play();render(smooth);smooth.track(0).gain=0;out=render(smooth);
    expect(out.getSample(0,0)>0.09f && out.getSample(0,239)<0.001f,"fader ramp avoids immediate discontinuity");
    std::cout<<checks<<" mixer checks passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}}
