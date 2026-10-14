#include "Session.h"
#include <cmath>
#include <map>
namespace nova {
static bool numeric(const juce::var& value) { return value.isDouble() || value.isInt() || value.isInt64(); }
bool Session::save(const juce::File& file, const AudioEngine& engine, juce::String& error) {
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("format", "nova-daw-session"); root->setProperty("version", 2);
    root->setProperty("masterGain", engine.masterGain.load());
    root->setProperty("bpm",engine.bpm); root->setProperty("beatsPerBar",engine.beatsPerBar); root->setProperty("beatUnit",engine.beatUnit);
    root->setProperty("loopEnabled",engine.loopEnabled.load()); root->setProperty("loopStart",engine.loopStart.load()); root->setProperty("loopEnd",engine.loopEnd.load());
    juce::Array<juce::var> rows;
    for (int i = 0; i < engine.trackCount(); ++i) {
        const auto& t = engine.track(i);
        auto row = std::make_unique<juce::DynamicObject>();
        row->setProperty("file", t.clip->source.getRelativePathFrom(file.getParentDirectory()));
        row->setProperty("name", t.name); row->setProperty("gain", t.gain.load());
        row->setProperty("pan", t.pan.load()); row->setProperty("mute", t.mute.load()); row->setProperty("solo", t.solo.load());
        row->setProperty("colour",juce::String::toHexString(static_cast<int64_t>(t.colour)));
        row->setProperty("height",t.height); row->setProperty("outputMode",t.outputMode.load());
        juce::Array<juce::var> regions;
        for(const auto& r:t.regions) {
            auto item=std::make_unique<juce::DynamicObject>();
            item->setProperty("file",r.asset->source.getRelativePathFrom(file.getParentDirectory()));
            item->setProperty("start",r.start); item->setProperty("offset",r.offset); item->setProperty("length",r.length);
            regions.add(juce::var(item.release()));
        }
        row->setProperty("regions",regions);
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
        || data["format"].toString() != "nova-daw-session" || !data["version"].isInt() || (static_cast<int>(data["version"]) != 1 && static_cast<int>(data["version"]) != 2)
        || !data["tracks"].isArray() || !numeric(data["masterGain"])) { error = "Sessao invalida ou versao nao suportada."; return false; }
    auto* rows = data["tracks"].getArray();
    if (rows->size() > AudioEngine::maxTracks) { error = "Sessao excede 64 faixas."; return false; }
    const bool v2=static_cast<int>(data["version"])==2;
    if(v2 && (!numeric(data["bpm"]) || !data["beatsPerBar"].isInt() || !data["beatUnit"].isInt()
        || !numeric(data["loopStart"]) || !numeric(data["loopEnd"]) || !data["loopEnabled"].isBool())) {
        error="Dados de tempo, compasso ou loop invalidos.";return false;
    }
    struct Entry { std::shared_ptr<Clip> clip; float gain, pan; bool mute, solo; juce::String name; uint32_t colour; int height, route; std::vector<Track::Region> regions; };
    std::vector<Entry> entries;
    int64_t total = 0;
    std::map<juce::String,std::shared_ptr<Clip>> assets;
    auto loadAsset=[&](const juce::var& path)->std::shared_ptr<Clip> {
        if(!path.isString() || path.toString().isEmpty()){error="Referencia de audio invalida.";return {};}
        const auto wav=file.getParentDirectory().getChildFile(path.toString()); const auto key=wav.getFullPathName();
        if(auto found=assets.find(key);found!=assets.end())return found->second;
        std::shared_ptr<Clip> asset=AudioEngine::loadWav(wav,error); if(!asset)return {};
        total+=static_cast<int64_t>(asset->samples.getNumSamples())*asset->samples.getNumChannels()*sizeof(float);
        if(total>AudioEngine::maxSessionBytes){error="Sessao excede 512 MiB de audio decodificado.";return {};}
        assets[key]=asset;return asset;
    };
    const double bpm=v2 ? static_cast<double>(data["bpm"]) : 120;
    const int beats=v2 ? static_cast<int>(data["beatsPerBar"]) : 4, unit=v2 ? static_cast<int>(data["beatUnit"]) : 4;
    const double ls=v2 ? static_cast<double>(data["loopStart"]) : 0, le=v2 ? static_cast<double>(data["loopEnd"]) : 0;
    if(!std::isfinite(bpm) || bpm<30 || bpm>300 || beats<1 || beats>12 || (unit!=4 && unit!=8)
        || !std::isfinite(ls) || !std::isfinite(le) || ls<0 || le<ls) {error="Tempo, compasso ou loop invalido.";return false;}
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
        auto clip = loadAsset(row["file"]);
        if (!clip) return false;
        Entry entry{clip,gain,pan,static_cast<bool>(row["mute"]),static_cast<bool>(row["solo"]),
            row["name"].isString() ? row["name"].toString() : clip->name,0xff55d8c2,140,0,{}};
        if(v2) {
            if(!row["regions"].isArray() || row["regions"].getArray()->size()>AudioEngine::maxRegionsPerTrack){error="Regioes invalidas ou limite de 128 por faixa excedido.";return false;}
            if(!row["height"].isInt() || !row["outputMode"].isInt() || !row["colour"].isString()
                || row["colour"].toString().length()!=8 || !row["colour"].toString().containsOnly("0123456789abcdefABCDEF")) {
                error="Altura, cor ou saida de faixa invalida.";return false;
            }
            entry.colour=static_cast<uint32_t>(row["colour"].toString().getHexValue64());
            entry.height=static_cast<int>(row["height"]);entry.route=static_cast<int>(row["outputMode"]);
            if(entry.height<100 || entry.height>320 || entry.route<0 || entry.route>2){error="Altura ou saida de faixa invalida.";return false;}
            for(const auto& item:*row["regions"].getArray()) {
                auto asset=loadAsset(item["file"]);if(!asset)return false;
                if(!numeric(item["start"]) || !numeric(item["offset"]) || !numeric(item["length"])){error="Tempos de clip invalidos.";return false;}
                const double start=static_cast<double>(item["start"]),offset=static_cast<double>(item["offset"]),length=static_cast<double>(item["length"]);
                if(!std::isfinite(start) || !std::isfinite(offset) || !std::isfinite(length) || start<0 || start>86400
                    || offset<0 || length<=0 || offset+length>asset->duration()+1e-8 || start+length>86400){error="Regiao fora dos limites de audio ou de sessao (24h).";return false;}
                entry.regions.push_back({0,asset,start,offset,length});
            }
        }else entry.regions.push_back({0,clip,0,0,clip->duration()});
        entries.push_back(std::move(entry));
    }
    double end=0;for(const auto& entry:entries)for(const auto& r:entry.regions)end=std::max(end,r.end());
    if(v2 && static_cast<bool>(data["loopEnabled"]) && (le<=ls || le>end+1e-8)){error="Loop fora da sessao.";return false;}
    engine.clear();
    for (auto& entry : entries) {
        engine.addSharedClip(entry.clip, error);
        auto& t = engine.track(engine.trackCount() - 1);
        t.gain = entry.gain; t.pan = entry.pan; t.mute = entry.mute; t.solo = entry.solo;
        t.name=entry.name;t.colour=entry.colour;t.height=entry.height;t.outputMode=entry.route;
        t.regions=std::move(entry.regions);for(auto& r:t.regions)r.id=engine.newRegionId();
    }
    engine.recalculateMediaBytes();engine.bpm=bpm;engine.beatsPerBar=beats;engine.beatUnit=unit;
    engine.setLoop(ls,le,v2 && static_cast<bool>(data["loopEnabled"]));
    engine.masterGain = master; return true;
}
}
