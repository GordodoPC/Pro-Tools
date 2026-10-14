#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI initialiser;
    if(argc!=3)return 2;
    try{
        juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;
        format.findAllTypesForFile(types,juce::String::fromUTF8(argv[1]));juce::KnownPluginList list;
        for(const auto* type:types)if(!type->isInstrument && type->numInputChannels==2 && type->numOutputChannels==2)list.addType(*type);
        if(auto xml=list.createXml())return juce::File(juce::String::fromUTF8(argv[2])).replaceWithText(xml->toString())?0:3;
    }catch(...){return 4;}
    return 5;
}
