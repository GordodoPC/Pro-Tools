#include "AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace nova {
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<double>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);

std::unique_ptr<Clip> AudioEngine::loadWav(const juce::File& file, juce::String& error) {
    if (!file.hasFileExtension("wav")) { error = "Importe um arquivo WAV mono ou estereo."; return {}; }
    juce::WavAudioFormat format;
    auto stream = file.createInputStream();
    if (!stream) { error = "Nao foi possivel abrir: " + file.getFullPathName(); return {}; }
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(), true));
    if (!reader || reader->lengthInSamples <= 0 || reader->sampleRate <= 0
        || !std::isfinite(reader->sampleRate) || reader->numChannels < 1 || reader->numChannels > 2) {
        error = "WAV invalido ou formato nao suportado (somente mono/estereo): " + file.getFileName(); return {};
    }
    if (reader->lengthInSamples > std::numeric_limits<int>::max()
        || reader->lengthInSamples * static_cast<int64_t>(reader->numChannels) * sizeof(float) > maxSessionBytes) {
        error = "O arquivo excede o limite de 512 MiB de audio decodificado."; return {};
    }
    auto clip = std::make_unique<Clip>();
    clip->source = file; clip->name = file.getFileNameWithoutExtension(); clip->sampleRate = reader->sampleRate;
    clip->samples.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&clip->samples, 0, clip->samples.getNumSamples(), 0, true, true)) {
        error = "Erro ao decodificar WAV: " + file.getFileName(); return {};
    }
    const int bins = std::min(4096, clip->samples.getNumSamples());
    clip->waveform.resize(static_cast<size_t>(bins));
    for (int i = 0; i < bins; ++i) {
        const int start = static_cast<int>(static_cast<int64_t>(i) * clip->samples.getNumSamples() / bins);
        const int end = static_cast<int>(static_cast<int64_t>(i + 1) * clip->samples.getNumSamples() / bins);
        Peak peak;
        for (int channel = 0; channel < clip->samples.getNumChannels(); ++channel) {
            auto* data = clip->samples.getReadPointer(channel);
            for (int sample = start; sample < end; ++sample) {
                if (!std::isfinite(data[sample])) { error = "WAV contem amostras nao finitas."; return {}; }
                peak.low = std::min(peak.low, data[sample]); peak.high = std::max(peak.high, data[sample]);
            }
        }
        clip->waveform[static_cast<size_t>(i)] = peak;
    }
    return clip;
}

bool AudioEngine::addClip(std::unique_ptr<Clip> clip, juce::String& error) {
    return addSharedClip(std::shared_ptr<Clip>(std::move(clip)),error);
}
void AudioEngine::recalculateMediaBytes() {
    std::set<const Clip*> assets;
    for(int i=0;i<count;++i) { assets.insert(track(i).clip.get()); for(const auto& r:track(i).regions)assets.insert(r.asset.get()); }
    bytes=0;
    for(const auto* asset:assets) if(asset) bytes+=static_cast<int64_t>(asset->samples.getNumSamples())*asset->samples.getNumChannels()*sizeof(float);
}
bool AudioEngine::addSharedClip(std::shared_ptr<Clip> clip, juce::String& error) {
    if (!clip) { error = "Clip vazio."; return false; }
    bool exists=false;
    for(int i=0;i<count;++i) { exists|=track(i).clip.get()==clip.get(); for(const auto& r:track(i).regions)exists|=r.asset.get()==clip.get(); }
    const int64_t clipBytes = exists ? 0 : static_cast<int64_t>(clip->samples.getNumChannels()) * clip->samples.getNumSamples() * sizeof(float);
    if (count == maxTracks || bytes + clipBytes > maxSessionBytes) {
        error = "Limite: 64 faixas e 512 MiB de audio decodificado por sessao."; return false;
    }
    auto& t = tracks[static_cast<size_t>(count++)];
    t.clip = std::move(clip); t.gain = 1; t.pan = 0; t.mute = false; t.solo = false; t.peak = 0;
    t.name = t.clip->name; t.height = 140; t.outputMode = 0;
    t.colour = juce::Colour::fromHSV(std::fmod(0.47f + (count - 1) * 0.13f, 1.0f), 0.5f, 0.88f, 1.0f).getARGB();
    t.regions = {{newRegionId(), t.clip, 0, 0, t.clip->duration()}};
    bytes += clipBytes; return true;
}
void AudioEngine::clear() {
    stop();
    for (auto& t : tracks) { t.regions.clear(); t.clip.reset(); }
    count = 0; bytes = 0; masterGain = 1;
    loopEnabled = false; loopStart = 0; loopEnd = 0; bpm = 120; beatsPerBar = beatUnit = 4;
}
double AudioEngine::duration() const {
    double end = 0;
    for (int i = 0; i < count; ++i) for (const auto& region : tracks[static_cast<size_t>(i)].regions) end = std::max(end, region.end());
    return end;
}
void AudioEngine::setLoop(double start, double end, bool enabled) {
    if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end <= start || end > duration() + 1e-8) {
        loopEnabled = false; return;
    }
    loopStart = start; loopEnd = end; loopEnabled = enabled;
}
void AudioEngine::seek(double seconds) {
    if (!std::isfinite(seconds)) return;
    const double value = std::clamp(seconds, 0.0, duration());
    requestedPosition.store(value, std::memory_order_relaxed);
    visiblePosition.store(value);
    seekGeneration.fetch_add(1, std::memory_order_release);
}
void AudioEngine::render(float* const* output, int channels, int frames) noexcept {
    for (int c = 0; c < channels; ++c) if (output[c]) std::fill_n(output[c], frames, 0.0f);
    const auto generation = seekGeneration.load(std::memory_order_acquire);
    if (generation != consumedSeek) {
        renderPosition = requestedPosition.load(std::memory_order_relaxed); consumedSeek = generation;
    }
    float masterLevel = 0;
    for (int i = 0; i < count; ++i) tracks[static_cast<size_t>(i)].peak = 0;
    if (!playing.load() || channels == 0 || frames <= 0) { masterPeak = 0; return; }
    bool anySolo = false;
    const double ls = loopStart.load(), le = loopEnd.load();
    const bool looping = loopEnabled.load() && le > ls && ls >= 0 && le <= duration() + 1e-8;
    auto timeAt = [&](int frame) {
        const double time = renderPosition + frame / outputRate;
        return looping && time >= le ? ls + std::fmod(time - ls, le - ls) : time;
    };
    for (int i = 0; i < count; ++i) anySolo |= tracks[static_cast<size_t>(i)].solo.load(std::memory_order_relaxed);
    for (int i = 0; i < count; ++i) {
        auto& t = tracks[static_cast<size_t>(i)];
        if (t.mute.load(std::memory_order_relaxed) || (anySolo && !t.solo.load(std::memory_order_relaxed))) continue;
        const auto gain = std::clamp(t.gain.load(std::memory_order_relaxed), 0.0f, 2.0f);
        const auto pan = std::clamp(t.pan.load(std::memory_order_relaxed), -1.0f, 1.0f);
        const int route = t.outputMode.load(std::memory_order_relaxed);
        float level = 0;
        for (const auto& region : t.regions) {
        const auto& clip = *region.asset;
        const bool mono = clip.samples.getNumChannels() == 1;
        const float leftGain = gain * (mono ? std::sqrt((1 - pan) * 0.5f) : (pan > 0 ? 1 - pan : 1));
        const float rightGain = gain * (mono ? std::sqrt((1 + pan) * 0.5f) : (pan < 0 ? 1 + pan : 1));
        const auto* left = clip.samples.getReadPointer(0);
        const auto* right = clip.samples.getReadPointer(mono ? 0 : 1);
        for (int frame = 0; frame < frames; ++frame) {
            const double time = timeAt(frame);
            if (time < region.start || time >= region.end()) continue;
            const double sourcePosition = (time - region.start + region.offset) * clip.sampleRate;
            if (sourcePosition < 0 || sourcePosition >= clip.samples.getNumSamples()) continue;
            const int index = static_cast<int>(sourcePosition);
            const int next = std::min(index + 1, clip.samples.getNumSamples() - 1);
            const float frac = static_cast<float>(sourcePosition - index);
            const float l = (left[index] + (left[next] - left[index]) * frac) * leftGain;
            const float r = (right[index] + (right[next] - right[index]) * frac) * rightGain;
            if (channels == 1) { if (output[0]) output[0][frame] += (l + r) * 0.5f; }
            else {
                if (output[0]) output[0][frame] += route == 2 ? 0 : route == 1 ? (l+r)*0.5f : l;
                if (output[1]) output[1][frame] += route == 1 ? 0 : route == 2 ? (l+r)*0.5f : r;
            }
            level = std::max(level, std::max(std::abs(l), std::abs(r)));
        }
        }
        t.peak = level;
    }
    const auto master = std::clamp(masterGain.load(std::memory_order_relaxed), 0.0f, 2.0f);
    for (int c = 0; c < channels; ++c) if (output[c]) for (int frame = 0; frame < frames; ++frame) {
        const float value = output[c][frame] * master;
        masterLevel = std::max(masterLevel, std::abs(value));
        output[c][frame] = std::clamp(value, -1.0f, 1.0f);
    }
    renderPosition = timeAt(frames);
    if (!looping && renderPosition >= duration()) { renderPosition = duration(); playing = false; }
    visiblePosition.store(renderPosition); masterPeak = masterLevel;
}
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*, int,
    float* const* out, int channels, int samples, const juce::AudioIODeviceCallbackContext&) {
    render(out, channels, samples);
}
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) { setOutputRate(device->getCurrentSampleRate()); }
void AudioEngine::audioDeviceStopped() { pause(); }
AudioRuntime::AudioRuntime() { initialError = devices.initialiseWithDefaultDevices(0, 2); devices.addAudioCallback(&engine); }
AudioRuntime::~AudioRuntime() { devices.removeAudioCallback(&engine); devices.closeAudioDevice(); }
juce::String AudioRuntime::deviceDescription() const {
    if (auto* device = devices.getCurrentAudioDevice())
        return device->getName() + " | " + juce::String(device->getCurrentSampleRate(), 0)
            + " Hz | " + juce::String(device->getCurrentBufferSizeSamples()) + " samples";
    return "Sem dispositivo de saida: " + initialError;
}
}
