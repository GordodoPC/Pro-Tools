#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "OfflineAudio.h"
#include "RegionAudio.h"
#include "Session/Session.h"
#include <cmath>
namespace nova {
namespace {
bool cancelled(const std::atomic<bool>* cancel,juce::String& error){if(cancel&&cancel->load()){error="Processamento cancelado; fontes preservadas.";return true;}return false;}
std::unique_ptr<juce::AudioFormatWriter> writer(const juce::File& file,double rate,int channels,juce::String& error){
    std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();if(!stream){error="Destino WAV indisponivel.";return {};}
    juce::WavAudioFormat format;auto result=format.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(channels).withBitsPerSample(32).withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    if(!result)error="Falha ao criar WAV float32.";return result;
}
bool publish(juce::TemporaryFile& temporary,const juce::File& destination,juce::String& error){if(destination.existsAsFile()){error="Destino existente: escolha nome novo.";return false;}if(!MoveFileW(temporary.getFile().getFullPathName().toWideCharPointer(),destination.getFullPathName().toWideCharPointer())){error="Falha ao publicar WAV; fontes preservadas.";return false;}return true;}
}
std::shared_ptr<Clip> OfflineAudio::transform(const Track::Region& region,double stretch,double semitones,double cents,const juce::File& cache,juce::String& error,const std::atomic<bool>* cancel,std::atomic<float>* progress){
    error.clear();if(!region.asset||!std::isfinite(stretch)||stretch<0.25||stretch>4||!std::isfinite(semitones)||std::abs(semitones)>12||!std::isfinite(cents)||std::abs(cents)>100){error="Stretch 0.25-4 e pitch +/-12 semitons/+/-100 cents exigidos.";return {};}
    const auto source=region.original?region.original:region.asset;const double offset=region.original?region.originalOffset:region.offset,length=region.original?region.originalLength:region.length;
    const int inputFrames=int(std::llround(length*source->sampleRate));const int64_t targetFrames=std::llround(inputFrames*stretch);const double pitch=std::pow(2.0,(semitones+cents/100)/12),factor=stretch*pitch;
    if(inputFrames<1||targetFrames<1||targetFrames>std::numeric_limits<int>::max()||targetFrames*source->samples.getNumChannels()*4>AudioEngine::maxSessionBytes){error="Resultado offline excede 512 MiB.";return {};}
    if(!cache.createDirectory().wasOk()){error="Cache indisponivel.";return {};}if(cancelled(cancel,error))return {};
    // Source is already decoded/immutable. FNV over sample bytes prevents stale hits
    // when a source file changes while retaining its name/mtime/size.
    uint64_t hash=14695981039346656037ull;for(int c=0;c<source->samples.getNumChannels();++c){const auto* bytes=reinterpret_cast<const unsigned char*>(source->samples.getReadPointer(c));const size_t count=size_t(source->samples.getNumSamples())*sizeof(float);for(size_t n=0;n<count;++n){if((n&65535)==0&&cancelled(cancel,error))return {};hash^=bytes[n];hash*=1099511628211ull;}if(cancelled(cancel,error))return {};}
    const auto key="nova-pv1/"+source->source.getFullPathName()+"/"+juce::String::toHexString(int64_t(hash))+"/"+juce::String(source->sampleRate,12)+"/"+juce::String(offset,12)+"/"+juce::String(length,12)+"/"+juce::String(stretch,12)+"/"+juce::String(semitones,12)+"/"+juce::String(cents,12);
    auto file=cache.getChildFile("processed-"+juce::String::toHexString(key.hashCode64())+".wav");auto metadata=file.withFileExtension("json");
    if(file.existsAsFile()&&metadata.loadFileAsString()==key){auto result=AudioEngine::loadWav(file,error);if(result&&result->samples.getNumSamples()==targetFrames){if(progress)*progress=1;return std::shared_ptr<Clip>(std::move(result));}}
    if(file.existsAsFile()){file=cache.getChildFile("processed-"+juce::Uuid().toString()+".wav");metadata=file.withFileExtension("json");}
    constexpr int order=11,fftSize=1<<order,hop=64;const double synthesisHop=hop*factor;
    const int64_t stretchedFrames=std::llround(inputFrames*factor);const int64_t workspaceFrames=stretchedFrames+int64_t(fftSize*factor)+fftSize*2;
    if(workspaceFrames>std::numeric_limits<int>::max()||workspaceFrames*8+targetFrames*source->samples.getNumChannels()*4>AudioEngine::maxSessionBytes){error="Workspace offline excede 512 MiB; reduza o trecho.";return {};}
    juce::dsp::FFT fft(order);std::array<float,fftSize> window{};for(int n=0;n<fftSize;++n)window[n]=float(0.5-0.5*std::cos(2*juce::MathConstants<double>::pi*n/fftSize));
    juce::AudioBuffer<float> output(source->samples.getNumChannels(),int(targetFrames));const int first=int(std::llround(offset*source->sampleRate));
    const int frameCount=(inputFrames+fftSize+hop-1)/hop;
    for(int channel=0;channel<output.getNumChannels();++channel){std::vector<float> overlap(size_t(workspaceFrames),0),normalization(size_t(workspaceFrames),0);std::array<float,fftSize*2> spectrum{};std::array<double,fftSize/2+1> previous{},phase{};
        for(int frame=0;frame<frameCount;++frame){if(cancelled(cancel,error))return {};const int analysis=frame*hop-fftSize/2;const int synthesis=int(std::llround(frame*synthesisHop));spectrum.fill(0);
            for(int n=0;n<fftSize;++n){const int sample=analysis+n;if(sample>=0&&sample<inputFrames&&first+sample>=0&&first+sample<source->samples.getNumSamples())spectrum[n]=source->samples.getSample(channel,first+sample)*window[n];}
            fft.performRealOnlyForwardTransform(spectrum.data(),true);
            for(int k=0;k<=fftSize/2;++k){const double real=spectrum[2*k],imag=spectrum[2*k+1],angle=std::atan2(imag,real),magnitude=std::hypot(real,imag),omega=2*juce::MathConstants<double>::pi*k/fftSize;
                double difference=angle-previous[k]-omega*hop;difference=std::remainder(difference,2*juce::MathConstants<double>::pi);if(frame==0)phase[k]=angle;else phase[k]+=(omega+difference/hop)*synthesisHop;previous[k]=angle;
                spectrum[2*k]=float(magnitude*std::cos(phase[k]));spectrum[2*k+1]=float(magnitude*std::sin(phase[k]));}
            fft.performRealOnlyInverseTransform(spectrum.data());
            for(int n=0;n<fftSize;++n)if(synthesis+n<int(workspaceFrames)){overlap[synthesis+n]+=spectrum[n]*window[n];normalization[synthesis+n]+=window[n]*window[n];}
            if(progress)*progress=float((channel+double(frame)/frameCount)/output.getNumChannels())*0.9f;
        }
        const double padding=(fftSize/2.0)*factor;
        auto sampleAt=[&](int n){return n>=0&&n<int(workspaceFrames)&&normalization[n]>1e-8f?overlap[n]/normalization[n]:0.0f;};
        for(int n=0;n<int(targetFrames);++n){if((n&4095)==0&&cancelled(cancel,error))return {};const double pos=padding+n*pitch;const int a=int(pos);const float value=sampleAt(a)+(sampleAt(a+1)-sampleAt(a))*float(pos-a);if(!std::isfinite(value)){error="Transformacao produziu sinal invalido.";return {};}output.setSample(channel,n,value);}
    }
    if(cancelled(cancel,error))return {};juce::TemporaryFile temporary(file);auto destination=writer(temporary.getFile(),source->sampleRate,output.getNumChannels(),error);if(!destination)return {};
    for(int pos=0;pos<output.getNumSamples();pos+=4096){if(cancelled(cancel,error))return {};const int frames=std::min(4096,output.getNumSamples()-pos);if(!destination->writeFromAudioSampleBuffer(output,pos,frames)){error="Falha de escrita offline.";return {};}if(progress)*progress=0.9f+0.1f*float(double(pos+frames)/output.getNumSamples());}destination.reset();if(cancelled(cancel,error)||!publish(temporary,file,error))return {};metadata.replaceWithText(key);if(progress)*progress=1;
    return std::shared_ptr<Clip>(AudioEngine::loadWav(file,error));
}
std::shared_ptr<Clip> OfflineAudio::consolidate(const std::vector<Track::Region>& regions,double start,double end,double rate,const juce::File& destination,juce::String& error,const std::atomic<bool>* cancel,std::atomic<float>* progress){
    error.clear();if(!std::isfinite(start)||!std::isfinite(end)||end<=start||start<0||end>86400||!std::isfinite(rate)||rate<100||rate>768000||regions.empty()){error="Selecao offline invalida.";return {};}
    const int64_t frames=std::llround((end-start)*rate);if(frames<1||frames*8>AudioEngine::maxSessionBytes){error="Consolidacao excede 512 MiB.";return {};}
    if(!destination.getParentDirectory().createDirectory().wasOk()||destination.existsAsFile()){error="Escolha novo arquivo WAV em pasta gravavel.";return {};}
    const int channels=std::all_of(regions.begin(),regions.end(),[](const auto& r){return r.asset&&r.asset->samples.getNumChannels()==1;})?1:2;
    juce::TemporaryFile temporary(destination);auto output=writer(temporary.getFile(),rate,channels,error);if(!output)return {};juce::AudioBuffer<float> block(2,mixBlockSize);
    for(int64_t pos=0;pos<frames;pos+=mixBlockSize){if(cancelled(cancel,error))return {};const int size=int(std::min<int64_t>(mixBlockSize,frames-pos));block.clear();for(int n=0;n<size;++n){const double time=start+(pos+n)/rate;for(const auto& region:regions)for(int c=0;c<channels;++c)block.addSample(c,n,regionSample(region,c,time-region.start)*(channels==2&&region.asset&&region.asset->samples.getNumChannels()==1?float(std::sqrt(0.5)):1.0f));}if(!output->writeFromAudioSampleBuffer(block,0,size)){error="Falha de escrita na consolidacao.";return {};}if(progress)*progress=float(double(pos+size)/frames);}
    output.reset();if(cancelled(cancel,error)||!publish(temporary,destination,error))return {};return std::shared_ptr<Clip>(AudioEngine::loadWav(destination,error));
}
std::unique_ptr<AudioEngine> OfflineAudio::snapshot(const AudioEngine& engine,const juce::File& directory,juce::String& error){
    try{error.clear();if(!directory.createDirectory().wasOk()){error="Pasta temporaria indisponivel.";return {};}juce::TemporaryFile temporary(directory.getChildFile("snapshot-"+juce::Uuid().toString()+".novadaw"));if(!Session::save(temporary.getFile(),engine,error))return {};
    auto offline=std::make_unique<AudioEngine>();offline->setOutputRate(engine.sampleRate());if(!Session::load(temporary.getFile(),*offline,error))return {};offline->setSmoothingTime(engine.smoothingSeconds());offline->loopEnabled=false;offline->metronomeEnabled=false;
    auto prepare=[](MixProcessor* processor){if(processor&&processor->plugin)processor->plugin->setNonRealtime(true);};for(int i=0;i<offline->trackCount();++i){prepare(offline->track(i).instrument.get());for(auto& p:offline->track(i).inserts)prepare(p.get());}for(auto& p:offline->masterInserts)prepare(p.get());offline->prepareProcessors();return offline;}catch(const std::bad_alloc&){error="Memoria insuficiente para preparar bounce; sessao preservada.";return {};}catch(...){error="Falha ao copiar estado para bounce; sessao preservada.";return {};}
}
bool OfflineAudio::bounce(AudioEngine& engine,double start,double end,const juce::File& destination,juce::String& error,const std::atomic<bool>* cancel,std::atomic<float>* progress){
    error.clear();if(!std::isfinite(start)||!std::isfinite(end)||start<0||end<=start||end>engine.duration()+1e-8||end>86400||!engine.pdcValid()){error="Intervalo de bounce invalido.";return false;}
    if(destination.existsAsFile()||!destination.getParentDirectory().createDirectory().wasOk()){error="Escolha novo arquivo WAV.";return false;}juce::TemporaryFile temporary(destination);auto output=writer(temporary.getFile(),engine.sampleRate(),2,error);if(!output)return false;
    engine.loopEnabled=false;engine.metronomeEnabled=false;engine.seek(start);engine.play();const int latency=engine.latencySamples();const int64_t frames=std::llround((end-start)*engine.sampleRate());if(frames<1||frames*8>=0xffff0000ull){error="Bounce excede RIFF classico.";return false;}juce::AudioBuffer<float> block(2,mixBlockSize);int64_t rendered=0;
    auto healthy=[](const MixProcessor* p){return !p||p->bypass||(!p->faulted&&(!p->plugin||!p->plugin->isSuspended())&&p->contentionDropouts()==0&&!(p->kind==EffectKind::vst3&&!p->plugin));};
    while(rendered<frames+latency){if(cancelled(cancel,error))return false;if(engine.needsMaintenance()){error="Latencia do plugin mudou durante bounce; tente novamente com estado estabilizado.";return false;}const int size=int(std::min<int64_t>(mixBlockSize,frames+latency-rendered));engine.render(block.getArrayOfWritePointers(),2,size);
        for(int i=0;i<engine.trackCount();++i){if(!healthy(engine.track(i).instrument.get())){error="Instrumento indisponivel/falhou no bounce.";return false;}for(auto& p:engine.track(i).inserts)if(!healthy(p.get())){error="Insert indisponivel/falhou no bounce.";return false;}}for(auto& p:engine.masterInserts)if(!healthy(p.get())){error="Master insert indisponivel/falhou no bounce.";return false;}
        const int skip=int(std::clamp<int64_t>(latency-rendered,0,size));if(size>skip&&!output->writeFromAudioSampleBuffer(block,skip,size-skip)){error="Erro de escrita no bounce.";return false;}rendered+=size;if(progress)*progress=float(double(rendered)/(frames+latency));
    }
    engine.stop();output.reset();return !cancelled(cancel,error)&&publish(temporary,destination,error);
}
}
