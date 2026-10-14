#pragma once
#include "MixProcessor.h"
namespace nova {
// Module probing runs in one disposable child process per VST3 bundle.
// Timer polls completion; no plugin scanner runs in the audio callback or parent process.
class PluginCatalog final : private juce::Timer {
public:
    explicit PluginCatalog(const juce::File& cache);
    ~PluginCatalog() override;
    juce::KnownPluginList known;
    void scan(const juce::FileSearchPath&);
    bool addDirectory(const juce::File&);
    juce::FileSearchPath configuredLocations() const;
    void scanConfigured() { scan(configuredLocations()); }
    void remove(const juce::PluginDescription&);
    void clearBlacklist();
    juce::String status() const { return scanStatus; }
    bool scanning() const { return active; }
    std::function<void()> onChanged;
private:
    juce::File cacheFile, resultFile, currentFile;
    juce::StringArray pending;
    juce::StringArray customDirectories;
    juce::ChildProcess child;
    bool active = false, childActive = false;
    double deadline = 0;
    int failures = 0;
    juce::String scanStatus = "Scanner VST3 pronto";
    void timerCallback() override;
    void saveCache();
};
}
