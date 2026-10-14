#include "Session.h"
#include <cmath>
namespace nova {
static bool numeric(const juce::var& value) { return value.isDouble() || value.isInt() || value.isInt64(); }
bool Session::save(const juce::File& file, const AudioEngine& engine, juce::String& error) {
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("format", "nova-daw-session"); root->setProperty("version", 1);
    root->setProperty("masterGain", engine.masterGain.load());
    juce::Array<juce::var> rows;
    for (int i = 0; i < engine.trackCount(); ++i) {
        const auto& t = engine.track(i);
        auto row = std::make_unique<juce::DynamicObject>();
        row->setProperty("file", t.clip->source.getRelativePathFrom(file.getParentDirectory()));
        row->setProperty("name", t.clip->name); row->setProperty("gain", t.gain.load());
        row->setProperty("pan", t.pan.load()); row->setProperty("mute", t.mute.load()); row->setProperty("solo", t.solo.load());
        rows.add(juce::var(row.release()));
    }
    root->setProperty("tracks", rows);
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(juce::JSON::toString(juce::var(root.release()), false))
        || !temporary.overwriteTargetFileWithTemporary()) { error = "Nao foi possivel salvar a sessao."; return false; }
    return true;
}
bool Session::load(const juce::File& file, AudioEngine& engine, juce::String& error) {
    juce::var data;
    if (juce::JSON::parse(file.loadFileAsString(), data).failed() || !data.isObject()
        || data["format"].toString() != "nova-daw-session" || static_cast<int>(data["version"]) != 1
        || !data["tracks"].isArray() || !numeric(data["masterGain"])) { error = "Sessao invalida ou versao nao suportada."; return false; }
    auto* rows = data["tracks"].getArray();
    if (rows->size() > AudioEngine::maxTracks) { error = "Sessao excede 64 faixas."; return false; }
    struct Entry { std::unique_ptr<Clip> clip; float gain, pan; bool mute, solo; };
    std::vector<Entry> entries;
    int64_t total = 0;
    const float master = static_cast<float>(data["masterGain"]);
    if (!std::isfinite(master) || master < 0 || master > 2) { error = "Ganho master invalido."; return false; }
    for (auto& row : *rows) {
        if (!row.isObject() || !row["file"].isString() || row["file"].toString().isEmpty()
            || !row["mute"].isBool() || !row["solo"].isBool()
            || !numeric(row["gain"]) || !numeric(row["pan"])) { error = "Dados de faixa invalidos."; return false; }
        const float gain = static_cast<float>(row["gain"]), pan = static_cast<float>(row["pan"]);
        if (!std::isfinite(gain) || !std::isfinite(pan) || gain < 0 || gain > 2 || pan < -1 || pan > 1) {
            error = "Volume ou pan invalido."; return false;
        }
        auto clip = AudioEngine::loadWav(file.getParentDirectory().getChildFile(row["file"].toString()), error);
        if (!clip) return false;
        total += static_cast<int64_t>(clip->samples.getNumSamples()) * clip->samples.getNumChannels() * sizeof(float);
        if (total > AudioEngine::maxSessionBytes) { error = "Sessao excede 512 MiB de audio decodificado."; return false; }
        if (row["name"].isString()) clip->name = row["name"].toString();
        entries.push_back({std::move(clip), gain, pan, static_cast<bool>(row["mute"]), static_cast<bool>(row["solo"])});
    }
    engine.clear();
    for (auto& entry : entries) {
        engine.addClip(std::move(entry.clip), error);
        auto& t = engine.track(engine.trackCount() - 1);
        t.gain = entry.gain; t.pan = entry.pan; t.mute = entry.mute; t.solo = entry.solo;
    }
    engine.masterGain = master; return true;
}
}
