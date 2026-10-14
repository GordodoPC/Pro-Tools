#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
// Original VST3 instrument fixture: MIDI controls a genuine audio oscillator.
// Fixed voices and channel state; no allocation or file access in processBlock.
class TestSynth final : public juce::AudioProcessor {
public:
    TestSynth():AudioProcessor(BusesProperties().withOutput("Out",juce::AudioChannelSet::stereo(),true)){
        addParameter(gain=new juce::AudioParameterFloat(juce::ParameterID("gain",1),"Gain",0.0f,1.0f,0.5f));channelGain.fill(1);
    }
    const juce::String getName()const override{return "NOVA Test Synth";}
    void prepareToPlay(double sampleRate,int)override{rate=sampleRate;reset();setLatencySamples(0);}
    void releaseResources()override{reset();}
    void reset()override{voices={};bend={};channelGain.fill(1);}
    bool isBusesLayoutSupported(const BusesLayout& layout)const override{return layout.getMainInputChannelSet().isDisabled()&&layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)override{
        buffer.clear();int cursor=0;
        for(const auto event:midi){const int at=std::clamp(event.samplePosition,cursor,buffer.getNumSamples());render(buffer,cursor,at);handle(event.getMessage());cursor=at;}
        render(buffer,cursor,buffer.getNumSamples());
    }
    bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}
    int getNumPrograms()override{return 2;}int getCurrentProgram()override{return program.load();}
    void setCurrentProgram(int index)override{program=std::clamp(index,0,1);}
    const juce::String getProgramName(int index)override{return index==0?"Sine":"Triangle";}
    void changeProgramName(int,const juce::String&)override{}
    bool hasEditor()const override{return true;}
    juce::AudioProcessorEditor* createEditor()override{return new juce::GenericAudioProcessorEditor(*this);}
    void getStateInformation(juce::MemoryBlock& data)override{juce::MemoryOutputStream out(data,false);out.writeFloat(gain->get());out.writeInt(program.load());}
    void setStateInformation(const void* data,int bytes)override{if(bytes>=8){juce::MemoryInputStream in(data,size_t(bytes),false);const float value=in.readFloat();if(std::isfinite(value))*gain=std::clamp(value,0.0f,1.0f);setCurrentProgram(in.readInt());}}
private:
    struct Voice{int pitch=-1,channel=0;double phase=0;float velocity=0;};
    std::array<Voice,64> voices{};std::array<float,16> channelGain{};std::array<double,16> bend{};
    juce::AudioParameterFloat* gain=nullptr;double rate=48000;std::atomic<int> program{0};
    void handle(const juce::MidiMessage& message){const int channel=message.getChannel()-1;if(channel<0||channel>15)return;
        if(message.isNoteOn()){
            for(auto& voice:voices)if(voice.pitch==message.getNoteNumber()&&voice.channel==channel)voice.pitch=-1;
            auto* chosen=&voices[0];for(auto& voice:voices)if(voice.pitch<0){chosen=&voice;break;}
            *chosen={message.getNoteNumber(),channel,0,message.getFloatVelocity()};
        }else if(message.isNoteOff()){for(auto& voice:voices)if(voice.pitch==message.getNoteNumber()&&voice.channel==channel)voice.pitch=-1;}
        else if(message.isAllNotesOff()||message.isAllSoundOff()){for(auto& voice:voices)if(voice.channel==channel)voice.pitch=-1;}
        else if(message.isController()&&message.getControllerNumber()==7)channelGain[channel]=message.getControllerValue()/127.0f;
        else if(message.isPitchWheel())bend[channel]=2.0*(message.getPitchWheelValue()-8192)/8192;
        else if(message.isProgramChange())setCurrentProgram(message.getProgramChangeNumber()%2);
    }
    void render(juce::AudioBuffer<float>& buffer,int first,int end){
        const auto twoPi=juce::MathConstants<double>::twoPi;
        for(int sample=first;sample<end;++sample){float value=0;for(auto& voice:voices)if(voice.pitch>=0){
            const double frequency=440*std::pow(2.0,(voice.pitch-69+bend[voice.channel])/12);
            const double wave=program==0?std::sin(voice.phase):2/juce::MathConstants<double>::pi*std::asin(std::sin(voice.phase));
            value+=float(wave)*0.15f*voice.velocity*channelGain[voice.channel]*gain->get();voice.phase=std::fmod(voice.phase+twoPi*frequency/rate,twoPi);
        }for(int channel=0;channel<buffer.getNumChannels();++channel)buffer.setSample(channel,sample,value);}
    }
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new TestSynth;}
