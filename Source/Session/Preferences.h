#pragma once
#include <juce_core/juce_core.h>
namespace nova {
struct Preferences {
    int trackWidth=250,clipWidth=190;
    double horizontalZoom=100,verticalZoom=1;
    bool mix=false,snap=true;
    void load(const juce::File&);
    bool save(const juce::File&) const;
};
}
