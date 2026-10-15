#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "Mastering.h"
#include "OfflineAudio.h"
#include <cmath>
#include <emmintrin.h>
namespace nova {
namespace {double dbEnergy(double e){return e>0?-0.691+10*std::log10(e):-INFINITY;}double dbPeak(double x){return x>0?20*std::log10(x):-INFINITY;}
bool aborted(const std::atomic<bool>* cancel,juce::String& error){if(cancel&&cancel->load()){error="Exportacao cancelada; fontes preservadas.";return true;}return false;}}
bool ExportOptions::valid()const{return std::isfinite(sampleRate)&&sampleRate>=8000&&sampleRate<=192000&&(bits==16||bits==24||bits==32)&&std::isfinite(targetLufs)&&targetLufs>=-70&&targetLufs<=0&&std::isfinite(ceilingDbTP)&&ceilingDbTP>=-24&&ceilingDbTP<=0;}
MasteringMeter::MasteringMeter(){for(int n=0;n<2048;++n)spectrumWindow[n]=float(0.5-0.5*std::cos(2*juce::MathConstants<double>::pi*n/2048));prepare(48000);}
void MasteringMeter::prepare(double rate){if(!std::isfinite(rate)||rate<100||rate>768000||rate==sampleRate)return;sampleRate=rate;hop=std::max(1,int(std::llround(rate*0.1)));
    // Original bilinear rate mapping of the two specified 48 kHz K filters.
    auto map=[&](std::array<double,3> b,std::array<double,3> a){const double u=48000/rate-1,v=48000/rate+1;auto convert=[&](auto c){return std::array<double,3>{c[0]*v*v+c[1]*u*v+c[2]*u*u,2*c[0]*u*v+c[1]*(u*u+v*v)+2*c[2]*u*v,c[0]*u*u+c[1]*u*v+c[2]*v*v};};Filter f;f.b=convert(b);f.a=convert(a);const double denominator=f.a[0];for(auto& x:f.b)x/=denominator;for(auto& x:f.a)x/=denominator;return f;};
    for(auto& channel:filters){channel[0]=map({1.53512485958697,-2.69169618940638,1.19839281085285},{1,-1.69065929318241,0.73248077421585});channel[1]=map({1,-2,1},{1,-1.99004745483398,0.99007225036621});}
    for(int phase=0;phase<4;++phase){double sum=0;for(int tap=0;tap<32;++tap){const double t=tap-16+phase*0.25;const double sinc=std::abs(t)<1e-12?1:std::sin(juce::MathConstants<double>::pi*t)/(juce::MathConstants<double>::pi*t);const double window=0.42-0.5*std::cos(2*juce::MathConstants<double>::pi*tap/31)+0.08*std::cos(4*juce::MathConstants<double>::pi*tap/31);sum+=(interpolation[phase][31-tap]=sinc*window);}for(auto& value:interpolation[phase])value/=sum;}
    reset();
}
void MasteringMeter::reset(){for(auto& c:filters)for(auto& f:c)f.z1=f.z2=0;peakHistory={};peakWrite=inHop=energyWrite=energyCount=spectrumWrite=0;hopEnergy=totalSquare=samplePeak=reconstructedPeak=0;sampleCount=0;energies={};blocks.clear();integratedDb=momentaryDb=peakDb=-INFINITY;crestDb=0;drops=0;double discarded;while(energyQueue.tryPop(discarded)){}std::array<float,2048> spectrumDiscarded;while(spectrumQueue.tryPop(spectrumDiscarded)){}bins.fill(-120);}
void MasteringMeter::interpolate(float left,float right)noexcept{
    peakHistory[0][peakWrite]=peakHistory[0][peakWrite+32]=left;
    peakHistory[1][peakWrite]=peakHistory[1][peakWrite+32]=right;
    for(int c=0;c<2;++c)for(int phase=0;phase<4;++phase){
        const auto* samples=peakHistory[c].data()+peakWrite+1;
        const auto* coefficients=interpolation[phase].data();
        auto a=_mm_setzero_pd(),b=a,d=a,e=a;
        for(int tap=0;tap<32;tap+=8){
            a=_mm_add_pd(a,_mm_mul_pd(_mm_loadu_pd(samples+tap),_mm_loadu_pd(coefficients+tap)));
            b=_mm_add_pd(b,_mm_mul_pd(_mm_loadu_pd(samples+tap+2),_mm_loadu_pd(coefficients+tap+2)));
            d=_mm_add_pd(d,_mm_mul_pd(_mm_loadu_pd(samples+tap+4),_mm_loadu_pd(coefficients+tap+4)));
            e=_mm_add_pd(e,_mm_mul_pd(_mm_loadu_pd(samples+tap+6),_mm_loadu_pd(coefficients+tap+6)));
        }
        double lanes[2];_mm_storeu_pd(lanes,_mm_add_pd(_mm_add_pd(a,b),_mm_add_pd(d,e)));
        reconstructedPeak=std::max(reconstructedPeak,std::abs(lanes[0]+lanes[1]));
    }
    peakWrite=(peakWrite+1)&31;
}
void MasteringMeter::process(const float* const* input,int channels,int frames)noexcept{
    juce::ScopedNoDenormals noDenormals;
    for(int n=0;n<frames;++n){float values[2]{};double energy=0;const int used=std::clamp(channels,1,2);for(int c=0;c<used;++c){const float x=input&&input[c]&&std::isfinite(input[c][n])?input[c][n]:0;values[c]=x;samplePeak=std::max(samplePeak,double(std::abs(x)));totalSquare+=double(x)*x;const double filtered=filters[c][1].run(filters[c][0].run(x));energy+=filtered*filtered;}sampleCount+=used;hopEnergy+=energy;interpolate(values[0],values[1]);
        spectrumInput[spectrumWrite++]=(values[0]+values[1])*0.5f;if(spectrumWrite==2048){if(!spectrumQueue.tryPush(spectrumInput))++drops;spectrumWrite=0;}
        if(++inHop==hop){energies[energyWrite]=hopEnergy/hop;energyWrite=(energyWrite+1)&3;energyCount=std::min(4,energyCount+1);if(energyCount==4){const double e=(energies[0]+energies[1]+energies[2]+energies[3])*0.25;momentaryDb=dbEnergy(e);if(!energyQueue.tryPush(e))++drops;}inHop=0;hopEnergy=0;peakDb=dbPeak(std::max(reconstructedPeak,samplePeak));crestDb=sampleCount&&totalSquare>0?dbPeak(samplePeak/std::sqrt(totalSquare/sampleCount)):0;}
    }
    peakDb=dbPeak(std::max(reconstructedPeak,samplePeak));
}
void MasteringMeter::finishTruePeak()noexcept{for(int n=0;n<32;++n)interpolate(0,0);peakDb=dbPeak(std::max(reconstructedPeak,samplePeak));}
void MasteringMeter::service(){double e;bool changed=false;for(int n=0;n<256&&energyQueue.tryPop(e);++n){if(blocks.size()<864000){blocks.push_back(e);changed=true;}else ++drops;}
    if(changed){double sum=0;size_t count=0;for(double energy:blocks)if(dbEnergy(energy)>-70){sum+=energy;++count;}const double threshold=count?std::max(std::pow(10.0,(-70+0.691)/10),sum/count*0.1):INFINITY;sum=0;count=0;for(double energy:blocks)if(energy>threshold){sum+=energy;++count;}integratedDb=count?dbEnergy(sum/count):-INFINITY;}
    std::array<float,2048> input;for(int frame=0;frame<4&&spectrumQueue.tryPop(input);++frame){std::array<float,4096> work{};for(int n=0;n<2048;++n)work[n]=input[n]*spectrumWindow[n];fft.performFrequencyOnlyForwardTransform(work.data());for(int bin=0;bin<1024;++bin)bins[bin]=float(dbPeak(std::max(1e-6f,work[bin]/512)));}
}
float MasteringExport::ditherSample(float value,int bits,uint64_t& random)noexcept{auto uniform=[&]{if(random==0)random=0x8f31a9d7a162b345ull;random^=random<<13;random^=random>>7;random^=random<<17;return double(random>>11)/9007199254740992.0;};if(bits==32)return value;return float(value+(uniform()-uniform())/std::ldexp(1.0,bits-1));}
bool MasteringExport::mix(AudioEngine& engine,double start,double end,const juce::File& destination,const ExportOptions& options,ExportResult& result,juce::String& error,const std::atomic<bool>* cancel,std::atomic<float>* progress){
    error.clear();if(!options.valid()||std::abs(options.sampleRate-engine.sampleRate())>0.001){error="Prepare o snapshot no sample rate de exportacao solicitado.";return false;}if(destination.exists()||!destination.getParentDirectory().createDirectory().wasOk()){error="Escolha novo WAV em pasta gravavel.";return false;}if(aborted(cancel,error))return false;
    juce::TemporaryFile raw(destination.getParentDirectory().getChildFile("raw-"+juce::Uuid().toString()+".wav"));if(!OfflineAudio::bounce(engine,start,end,raw.getFile(),error,cancel,nullptr))return false;if(progress)*progress=0.4f;
    juce::WavAudioFormat format;auto input=raw.getFile().createInputStream();std::unique_ptr<juce::AudioFormatReader> reader(input?format.createReaderFor(input.release(),true):nullptr);if(!reader){error="Falha ao ler mix offline.";return false;}MasteringMeter meter;meter.prepare(options.sampleRate);juce::AudioBuffer<float> block(2,4096);
    for(int64_t position=0;position<reader->lengthInSamples;position+=4096){if(aborted(cancel,error))return false;const int frames=int(std::min<int64_t>(4096,reader->lengthInSamples-position));if(!reader->read(&block,0,frames,position,true,true)){error="Falha de leitura na medicao.";return false;}meter.process(block.getArrayOfReadPointers(),2,frames);meter.service();if(progress)*progress=0.4f+0.2f*float(double(position+frames)/reader->lengthInSamples);}meter.finishTruePeak();meter.service();
    result={destination,meter.integrated(),meter.truePeak(),0,meter.integrated()};if(meter.dropped()){error="Medicao incompleta; exportacao abortada.";return false;}
    if(options.normalize){if(!std::isfinite(result.measuredLufs)){error="Intervalo silencioso ou menor que 400 ms; loudness indisponivel.";return false;}result.gainDb=std::min(options.targetLufs-result.measuredLufs,options.ceilingDbTP-result.truePeakDb);result.achievedLufs=result.measuredLufs+result.gainDb;}
    const double gain=std::pow(10.0,result.gainDb/20);juce::TemporaryFile temporary(destination);std::unique_ptr<juce::OutputStream> stream=temporary.getFile().createOutputStream();if(!stream){error="Destino de exportacao indisponivel.";return false;}
    auto writer=format.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(options.sampleRate).withNumChannels(2).withBitsPerSample(options.bits).withSampleFormat(options.bits==32?juce::AudioFormatWriterOptions::SampleFormat::floatingPoint:juce::AudioFormatWriterOptions::SampleFormat::integral));if(!writer){error="Falha ao preparar formato WAV.";return false;}uint64_t random=uint64_t(destination.getFullPathName().hashCode64())^0x8f31a9d7a162b345ull;
    for(int64_t position=0;position<reader->lengthInSamples;position+=4096){if(aborted(cancel,error))return false;const int frames=int(std::min<int64_t>(4096,reader->lengthInSamples-position));if(!reader->read(&block,0,frames,position,true,true)){error="Erro de leitura na exportacao.";return false;}for(int c=0;c<2;++c)for(int n=0;n<frames;++n){float value=float(block.getSample(c,n)*gain);if(options.dither&&options.bits!=32)value=ditherSample(value,options.bits,random);block.setSample(c,n,std::clamp(value,-1.0f,1.0f));}if(!writer->writeFromAudioSampleBuffer(block,0,frames)){error="Erro ao escrever WAV exportado.";return false;}if(progress)*progress=0.6f+0.4f*float(double(position+frames)/reader->lengthInSamples);}
    if(!writer->flush()){error="Falha ao finalizar cabecalho WAV.";return false;}writer.reset();if(aborted(cancel,error))return false;if(!MoveFileW(temporary.getFile().getFullPathName().toWideCharPointer(),destination.getFullPathName().toWideCharPointer())){error="Falha ao publicar WAV; nenhum destino existente foi substituido.";return false;}return true;
}
bool MasteringExport::stems(AudioEngine& engine,double start,double end,const juce::File& directory,const ExportOptions& options,std::vector<ExportResult>& results,juce::String& error,const std::atomic<bool>* cancel,std::atomic<float>* progress){
    if(!directory.createDirectory().wasOk()){error="Pasta de stems indisponivel.";return false;}const int previous=engine.stemSource.load();struct Restore{AudioEngine& engine;int previous;~Restore(){engine.stemSource=previous;}} restore{engine,previous};
    for(int i=0;i<engine.trackCount();++i)if(!engine.track(i).auxiliary){if(aborted(cancel,error))return false;engine.stemSource=i;ExportResult result;const auto file=directory.getChildFile("stem-"+juce::String(i+1).paddedLeft('0',2)+"-"+juce::File::createLegalFileName(engine.track(i).name).substring(0,40)+"-"+juce::Uuid().toString()+".wav");if(!mix(engine,start,end,file,options,result,error,cancel,progress))return false;results.push_back(std::move(result));}return !results.empty();
}
}
