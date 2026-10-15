#pragma once
#include <juce_core/juce_core.h>
namespace nova {
struct AppPaths {
    static constexpr const char* releaseName="NOVA DAW Alpha 0.7 - Public Test";
    static juce::File data(){const auto override=juce::SystemStats::getEnvironmentVariable("NOVA_DAW_DATA_DIR",{});const auto local=juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA",juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getFullPathName());return juce::File::isAbsolutePath(override)?juce::File(override):juce::File(local).getChildFile("NOVA DAW/Alpha 0.7");}
    static juce::File workspace(){const auto path=data().getChildFile("Workspace");path.createDirectory();return path;}
    static juce::File testSynth(){const auto exe=juce::File::getSpecialLocation(juce::File::currentExecutableFile);const auto bundled=exe.getSiblingFile("Plugins/NOVA Test Synth.vst3/Contents/x86_64-win/NOVA Test Synth.vst3");if(bundled.existsAsFile())return bundled;
        const auto config=exe.getParentDirectory();if(config.getParentDirectory().getFileName()=="NovaDAW_artefacts")return config.getParentDirectory().getParentDirectory().getChildFile("NovaTestSynth_artefacts/"+config.getFileName()+"/VST3/NOVA Test Synth.vst3/Contents/x86_64-win/NOVA Test Synth.vst3");return bundled;}
};
}
