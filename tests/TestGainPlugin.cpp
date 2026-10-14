#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdlib>
// Original deterministic VST3 fixture. Built locally; never installed globally.
class TestGain final : public juce::AudioProcessor {
public:
    TestGain():AudioProcessor(BusesProperties().withInput("In",juce::AudioChannelSet::stereo(),true).withOutput("Out",juce::AudioChannelSet::stereo(),true)){
        // Fault injection only in this original test fixture; no audio is simulated.
        if(juce::SystemStats::getEnvironmentVariable("NOVA_TEST_SCAN_FAILURE",{}).isNotEmpty())std::_Exit(86);
        addParameter(gain=new juce::AudioParameterFloat(juce::ParameterID("gain",1),"Gain",0.0f,1.0f,0.5f));
        addParameter(latency=new juce::AudioParameterInt(juce::ParameterID("latency",1),"Latency samples",0,2048,0));}
    const juce::String getName()const override{return "NOVA Test Gain";}
    void prepareToPlay(double,int)override{history.setSize(2,4096);history.clear();write=0;setLatencySamples(latency->get());}
    void releaseResources()override{
        const auto eventName=juce::SystemStats::getEnvironmentVariable("NOVA_TEST_LIFECYCLE_EVENT",{});
        if(eventName.isNotEmpty()){auto event=OpenEventW(EVENT_MODIFY_STATE,FALSE,eventName.toWideCharPointer());if(event){SetEvent(event);CloseHandle(event);}juce::Thread::sleep(150);}
    }
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)override{
        const int delay=latency->get();if(delay!=getLatencySamples())setLatencySamples(delay);
        for(int s=0;s<buffer.getNumSamples();++s){const int read=(write-delay+4096)%4096;
            for(int c=0;c<buffer.getNumChannels();++c){const float value=buffer.getSample(c,s)*gain->get();history.setSample(c,write,value);buffer.setSample(c,s,delay==0?value:history.getSample(c,read));}
            write=(write+1)%4096;}
    }
    bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainInputChannelSet()==juce::AudioChannelSet::stereo() && l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    juce::AudioProcessorEditor* createEditor()override{return new juce::GenericAudioProcessorEditor(*this);}
    bool hasEditor()const override{return true;}
    bool acceptsMidi()const override{return false;}
    bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}
    int getNumPrograms()override{return 1;}
    int getCurrentProgram()override{return 0;}
    void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {};}
    void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock& block)override{juce::MemoryOutputStream out(block,false);out.writeFloat(gain->get());out.writeInt(latency->get());}
    void setStateInformation(const void* data,int bytes)override{if(bytes>=4){juce::MemoryInputStream in(data,size_t(bytes),false);const float value=in.readFloat();if(std::isfinite(value))gain->setValueNotifyingHost(std::clamp(value,0.0f,1.0f));
        if(bytes>=8){*latency=std::clamp(in.readInt(),0,2048);setLatencySamples(latency->get());}}}
private:juce::AudioParameterFloat* gain=nullptr;juce::AudioParameterInt* latency=nullptr;juce::AudioBuffer<float> history;int write=0;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new TestGain;}
