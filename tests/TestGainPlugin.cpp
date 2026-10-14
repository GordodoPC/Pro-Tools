#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdlib>
// Original deterministic VST3 fixture. Built locally; never installed globally.
class TestGain final : public juce::AudioProcessor {
public:
    TestGain():AudioProcessor(BusesProperties().withInput("In",juce::AudioChannelSet::stereo(),true).withOutput("Out",juce::AudioChannelSet::stereo(),true)){
        // Fault injection only in this original test fixture; no audio is simulated.
        if(juce::SystemStats::getEnvironmentVariable("NOVA_TEST_SCAN_FAILURE",{}).isNotEmpty())std::_Exit(86);
        addParameter(gain=new juce::AudioParameterFloat(juce::ParameterID("gain",1),"Gain",0.0f,1.0f,0.5f));}
    const juce::String getName()const override{return "NOVA Test Gain";}
    void prepareToPlay(double,int)override{}
    void releaseResources()override{}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)override{buffer.applyGain(gain->get());}
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
    void getStateInformation(juce::MemoryBlock& block)override{juce::MemoryOutputStream out(block,false);out.writeFloat(gain->get());}
    void setStateInformation(const void* data,int bytes)override{if(bytes>=4){const float value=juce::MemoryInputStream(data,size_t(bytes),false).readFloat();if(std::isfinite(value))gain->setValueNotifyingHost(std::clamp(value,0.0f,1.0f));}}
private:juce::AudioParameterFloat* gain=nullptr;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new TestGain;}
