#include "Preferences.h"
#include <cmath>
namespace nova {
void Preferences::load(const juce::File& file){const auto data=juce::JSON::parse(file.loadFileAsString());if(!data.isObject()||static_cast<int>(data["version"])!=1)return;
    auto numeric=[](const juce::var& value){return value.isInt()||value.isInt64()||value.isDouble();};
    if(numeric(data["trackWidth"]))trackWidth=juce::jlimit(210,420,static_cast<int>(data["trackWidth"]));
    if(numeric(data["clipWidth"]))clipWidth=juce::jlimit(140,320,static_cast<int>(data["clipWidth"]));
    const double h=static_cast<double>(data["horizontalZoom"]),v=static_cast<double>(data["verticalZoom"]);
    if(numeric(data["horizontalZoom"])&&std::isfinite(h))horizontalZoom=juce::jlimit(20.0,600.0,h);
    if(numeric(data["verticalZoom"])&&std::isfinite(v))verticalZoom=juce::jlimit(0.65,2.0,v);
    if(data["mix"].isBool())mix=static_cast<bool>(data["mix"]);
    if(data["snap"].isBool())snap=static_cast<bool>(data["snap"]);
}
bool Preferences::save(const juce::File& file)const{auto data=std::make_unique<juce::DynamicObject>();data->setProperty("version",1);
    data->setProperty("trackWidth",trackWidth);data->setProperty("clipWidth",clipWidth);data->setProperty("horizontalZoom",horizontalZoom);data->setProperty("verticalZoom",verticalZoom);data->setProperty("mix",mix);data->setProperty("snap",snap);
    juce::TemporaryFile temporary(file);
    return temporary.getFile().replaceWithText(juce::JSON::toString(juce::var(data.release()),false))&&temporary.overwriteTargetFileWithTemporary();}
}
