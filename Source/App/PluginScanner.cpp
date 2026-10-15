#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
int wmain(int argc,wchar_t** argv){
    juce::ScopedJuceInitialiser_GUI initialiser;
    if(argc!=3)return 2;
    try{
        juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;
        format.findAllTypesForFile(types,juce::String(argv[1]));juce::KnownPluginList list;
        for(const auto* type:types){
            // Metadata may come from moduleinfo.json. Probe construction in this child too,
            // so constructors that fail are never first exercised in the session process.
            juce::String error;auto instance=format.createInstanceFromDescription(*type,48000,256,error);
            if(!instance)return 6;
            auto description=*type;
            description.numInputChannels=instance->getTotalNumInputChannels();
            description.numOutputChannels=instance->getTotalNumOutputChannels();
            if(description.numOutputChannels!=2 || (description.isInstrument?(description.numInputChannels!=0||!instance->acceptsMidi()):description.numInputChannels!=2))continue;
            instance->prepareToPlay(48000,256);instance->releaseResources();list.addType(description);
        }
        if(auto xml=list.createXml())return juce::File(juce::String(argv[2])).replaceWithText(xml->toString())?0:3;
    }catch(...){return 4;}
    return 5;
}
