#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Midi/MidiQueue.h"
#include <cmath>
namespace nova {
class AudioEngine;
struct ExportOptions {
    double sampleRate=48000,targetLufs=-14,ceilingDbTP=-1;
    int bits=24;bool normalize=false,dither=true;
    bool valid()const;
};
struct ExportResult {juce::File file;double measuredLufs=-INFINITY,truePeakDb=-INFINITY,gainDb=0,achievedLufs=-INFINITY;};
class MasteringMeter {
public:
    MasteringMeter();
    void prepare(double sampleRate);
    void reset(); // control thread, audio quiesced
    void process(const float* const*,int channels,int frames)noexcept;
    void finishTruePeak()noexcept;
    void service(); // UI/offline worker, never render
    double integrated()const{return integratedDb.load();}
    double momentary()const{return momentaryDb.load();}
    double truePeak()const{return peakDb.load();}
    double crest()const{return crestDb.load();}
    uint64_t dropped()const{return drops.load();}
    const std::array<float,1024>& spectrum()const{return bins;}
    double rate()const{return sampleRate;}
    size_t ownedBytes()const{return sizeof(*this)+blocks.capacity()*sizeof(double);}
private:
    struct Filter {std::array<double,3> b{1,0,0},a{1,0,0};double z1=0,z2=0;double run(double x)noexcept{const double y=b[0]*x+z1;z1=b[1]*x-a[1]*y+z2;z2=b[2]*x-a[2]*y;return y;}};
    std::array<std::array<Filter,2>,2> filters;
    std::array<std::array<double,32>,4> interpolation{};
    std::array<std::array<double,64>,2> peakHistory{}; // mirrored, contiguous 32-sample FIR window
    int peakWrite=0,hop=4800,inHop=0,energyWrite=0,energyCount=0,spectrumWrite=0;
    double sampleRate=0,hopEnergy=0,totalSquare=0,samplePeak=0,reconstructedPeak=0;uint64_t sampleCount=0;
    std::array<double,4> energies{};
    std::array<float,2048> spectrumInput{};
    std::array<float,2048> spectrumWindow{};
    MidiQueue<double,256> energyQueue;
    MidiQueue<std::array<float,2048>,4> spectrumQueue;
    std::vector<double> blocks;
    std::array<float,1024> bins{};
    juce::dsp::FFT fft{11};
    std::atomic<double> integratedDb{-INFINITY},momentaryDb{-INFINITY},peakDb{-INFINITY},crestDb{0};
    std::atomic<uint64_t> drops{0};
    void interpolate(float left,float right)noexcept;
};
class MasteringExport {
public:
    // Caller creates snapshot and prepares requested rate on the control thread.
    static bool mix(AudioEngine& offline,double start,double end,const juce::File&,const ExportOptions&,ExportResult&,juce::String&,const std::atomic<bool>* cancel=nullptr,std::atomic<float>* progress=nullptr);
    static bool stems(AudioEngine& offline,double start,double end,const juce::File& directory,const ExportOptions&,std::vector<ExportResult>&,juce::String&,const std::atomic<bool>* cancel=nullptr,std::atomic<float>* progress=nullptr);
    static float ditherSample(float value,int bits,uint64_t& random)noexcept;
};
}
