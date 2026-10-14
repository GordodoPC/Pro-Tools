#include "RegionAudio.h"
#include <cmath>
namespace nova {
float regionEnvelope(const Track::Region& r,double time) noexcept {
    auto curve=[](double x,FadeCurve shape){x=std::clamp(x,0.0,1.0);if(shape==FadeCurve::exponential)return float(x*x);if(shape==FadeCurve::equalPower)return float(std::sin(x*juce::MathConstants<double>::halfPi));return float(x);};
    if(time<0||time>=r.length)return 0;
    const float in=r.fadeIn>0?curve(time/r.fadeIn,r.fadeInCurve):1;
    const float out=r.fadeOut>0?curve((r.length-time)/r.fadeOut,r.fadeOutCurve):1;
    return in*out;
}
float regionSample(const Track::Region& r,int channel,double time) noexcept {
    if(!r.asset||time<0||time>=r.length)return 0;const auto& clip=*r.asset;
    const double pos=(time+r.offset)*clip.sampleRate;if(pos<0||pos>=clip.samples.getNumSamples())return 0;
    const int a=int(pos),b=std::min(a+1,clip.samples.getNumSamples()-1),c=std::min(channel,clip.samples.getNumChannels()-1);const float f=float(pos-a);
    const auto* samples=clip.samples.getReadPointer(c);return (samples[a]+(samples[b]-samples[a])*f)*regionEnvelope(r,time);
}
}
