#include "PluginCatalog.h"
namespace nova {
PluginCatalog::PluginCatalog(const juce::File& file):cacheFile(file){if(auto xml=juce::parseXML(cacheFile)){known.recreateFromXml(*xml);
    if(auto* paths=xml->getChildByName("NOVA_SCAN_PATHS"))for(auto* path:paths->getChildIterator())if(path->hasTagName("DIRECTORY"))customDirectories.addIfNotAlreadyThere(path->getStringAttribute("path"));}}
PluginCatalog::~PluginCatalog(){stopTimer();if(childActive)child.kill();resultFile.deleteFile();}
void PluginCatalog::saveCache(){if(auto xml=known.createXml()){auto* paths=xml->createNewChildElement("NOVA_SCAN_PATHS");for(const auto& path:customDirectories)paths->createNewChildElement("DIRECTORY")->setAttribute("path",path);
    juce::TemporaryFile temporary(cacheFile);if(temporary.getFile().replaceWithText(xml->toString()))temporary.overwriteTargetFileWithTemporary();}}
bool PluginCatalog::addDirectory(const juce::File& directory){if(!directory.isDirectory())return false;customDirectories.addIfNotAlreadyThere(directory.getFullPathName());saveCache();return true;}
juce::FileSearchPath PluginCatalog::configuredLocations()const {juce::VST3PluginFormat format;auto paths=format.getDefaultLocationsToSearch();for(const auto& directory:customDirectories)paths.add(juce::File(directory));paths.removeRedundantPaths();return paths;}
void PluginCatalog::remove(const juce::PluginDescription& description){known.removeType(description);saveCache();if(onChanged)onChanged();}
void PluginCatalog::clearBlacklist(){known.clearBlacklistedFiles();saveCache();if(onChanged)onChanged();}
void PluginCatalog::scan(const juce::FileSearchPath& paths){
    if(active)return;
    juce::VST3PluginFormat format;pending=format.searchPathsForPlugins(paths,true,false);
    failures=0;active=true;scanStatus="VST3: "+juce::String(pending.size())+" modulos na fila";startTimer(100);
}
void PluginCatalog::timerCallback(){
    if(childActive){
        if(child.isRunning()){
            if(juce::Time::getMillisecondCounterHiRes()<deadline)return;
            child.kill();++failures;known.addToBlacklist(currentFile.getFullPathName());
        }else {
            auto xml=juce::parseXML(resultFile);
            if(child.getExitCode()==0 && xml){juce::KnownPluginList list;list.recreateFromXml(*xml);for(const auto& description:list.getTypes())known.addType(description);known.removeFromBlacklist(currentFile.getFullPathName());}
            else{++failures;known.addToBlacklist(currentFile.getFullPathName());}
        }
        childActive=false;resultFile.deleteFile();saveCache();if(onChanged)onChanged();
    }
    if(pending.isEmpty()){active=false;stopTimer();scanStatus="VST3: "+juce::String(known.getNumTypes())+" efeitos, "+juce::String(failures)+" falhas";if(onChanged)onChanged();return;}
    currentFile=juce::File(pending[0]);pending.remove(0);resultFile=juce::File::createTempFile(".nova-scan.xml");
    const auto scanner=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("NovaPluginScanner.exe");
    scanStatus="VST3: examinando "+currentFile.getFileName();
    if(!scanner.existsAsFile() || !child.start(juce::StringArray{scanner.getFullPathName(),currentFile.getFullPathName(),resultFile.getFullPathName()},0)){
        ++failures;scanStatus="Scanner VST3 ausente ou falha ao iniciar";return;}
    childActive=true;deadline=juce::Time::getMillisecondCounterHiRes()+20000;
}
}
