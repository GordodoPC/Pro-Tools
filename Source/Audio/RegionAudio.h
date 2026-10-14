#pragma once
#include "AudioEngine.h"
namespace nova {
// Shared by realtime playback, offline bounce and curve drawing; no state/allocation.
float regionEnvelope(const Track::Region&,double localTime) noexcept;
float regionSample(const Track::Region&,int channel,double localTime) noexcept;
}
