#include "Session.h"
#include "AutomationModel.h"
#include <cmath>
#include <map>
#include <set>
namespace nova {
static bool numeric(const juce::var& value) { return value.isDouble() || value.isInt() || value.isInt64(); }
namespace {
juce::var insertState(const std::array<std::unique_ptr<MixProcessor>,4>& inserts){juce::Array<juce::var> values;for(const auto& p:inserts)values.add(p?p->state():juce::var());return values;}
bool readInserts(const juce::var& value,std::array<std::unique_ptr<MixProcessor>,4>& inserts,double rate,juce::String& error){
    if(!value.isArray() || value.getArray()->size()!=4){error="Slots de insert invalidos.";return false;}
    for(int i=0;i<4;++i){const auto& item=value.getArray()->getReference(i);if(item.isVoid())continue;
        inserts[i]=MixProcessor::restore(item,rate,error);if(!inserts[i])return false;}
    return true;
}
juce::var mixState(const Track& t){auto o=std::make_unique<juce::DynamicObject>();o->setProperty("inputGain",t.inputGain.load());o->setProperty("outputTarget",t.outputTarget);
    o->setProperty("inserts",insertState(t.inserts));juce::Array<juce::var> sends;
    for(const auto& send:t.sends){auto v=std::make_unique<juce::DynamicObject>();v->setProperty("target",send.target);v->setProperty("preFader",send.preFader);v->setProperty("gain",send.gain.load());sends.add(juce::var(v.release()));}
    o->setProperty("sends",sends);o->setProperty("automationMode",int(t.automationMode));juce::Array<juce::var> lanes;
    for(const auto& lane:t.automation){auto l=std::make_unique<juce::DynamicObject>();l->setProperty("target",lane.target);l->setProperty("parameter",lane.parameter);juce::Array<juce::var> points;
        for(const auto& point:lane.points){auto v=std::make_unique<juce::DynamicObject>();v->setProperty("time",point.time);v->setProperty("value",point.value);v->setProperty("curve",point.curve);points.add(juce::var(v.release()));}
        l->setProperty("points",points);lanes.add(juce::var(l.release()));}
    o->setProperty("automation",lanes);return juce::var(o.release());
}
bool readMix(const juce::var& v,Track& t,double rate,juce::String& error){
    if(!v.isObject() || !numeric(v["inputGain"]) || !std::isfinite(double(v["inputGain"])) || double(v["inputGain"])<0 || double(v["inputGain"])>16
        || !v["outputTarget"].isInt() || !v["sends"].isArray() || v["sends"].getArray()->size()!=2
        || !v["automationMode"].isInt() || int(v["automationMode"])<0 || int(v["automationMode"])>4
        || !v["automation"].isArray() || v["automation"].getArray()->size()>64){error="Mixagem/automacao invalida; modo/limites invalidos.";return false;}
    t.inputGain=float(v["inputGain"]);t.outputTarget=int(v["outputTarget"]);t.automationMode=static_cast<AutomationMode>(int(v["automationMode"]));
    if(!readInserts(v["inserts"],t.inserts,rate,error))return false;
    for(int i=0;i<2;++i){const auto& send=v["sends"].getArray()->getReference(i);
        if(!send.isObject() || !send["target"].isInt() || !send["preFader"].isBool() || !numeric(send["gain"]) || !std::isfinite(double(send["gain"])) || double(send["gain"])<0 || double(send["gain"])>2){error="Send invalido.";return false;}
        t.sends[i].target=int(send["target"]);t.sends[i].preFader=bool(send["preFader"]);t.sends[i].gain=float(send["gain"]);
    }
    std::set<std::pair<int,int>> identities;
    for(const auto& item:*v["automation"].getArray()){
        if(!item.isObject() || !item["target"].isInt() || !item["parameter"].isInt() || !item["points"].isArray() || item["points"].getArray()->size()>4096){error="Lane de automacao invalida.";return false;}
        AutomationLane lane;lane.target=int(item["target"]);lane.parameter=int(item["parameter"]);
        if(lane.target<-2 || lane.target>3 || lane.parameter<0 || lane.parameter>65535 || (lane.target>=0 && !t.inserts[lane.target])
            || !identities.insert({lane.target,lane.parameter}).second){error="Destino de automacao invalido/duplicado.";return false;}
        float low=lane.target==-2?-1.0f:0.0f,high=lane.target==-2?1.0f:2.0f;
        if(lane.target>=0){const auto& p=t.inserts[lane.target];if(p->kind!=EffectKind::vst3 || p->plugin){if(lane.parameter>=p->parameterCount()){error="Parametro de automacao inexistente.";return false;}const auto spec=p->spec(lane.parameter);low=spec.minimum;high=spec.maximum;}else high=1;}
        double previous=-1;
        for(const auto& point:*item["points"].getArray()){double time=double(point["time"]);float value=float(point["value"]);
            if(!numeric(point["time"]) || !numeric(point["value"]) || !std::isfinite(time) || !std::isfinite(value) || time<0 || time>86400 || time<=previous || value<low || value>high){error="Ponto de automacao invalido.";return false;}
            const int curve=point["curve"].isVoid()?0:int(point["curve"]);if(!point["curve"].isVoid()&&(!point["curve"].isInt()||curve<0||curve>2)){error="Curva de automacao invalida.";return false;}previous=time;lane.points.push_back({time,value,curve});}
        t.automation.push_back(std::move(lane));
    }
    return true;
}
}
bool Session::save(const juce::File& file, const AudioEngine& engine, juce::String& error) {
    engine.automation->finishAll();auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("format", "nova-daw-session"); root->setProperty("version", 7);root->setProperty("midiPpq",midiPpq);root->setProperty("metronome",engine.metronomeEnabled.load());
    const auto& record=engine.recordingOptions;auto settings=std::make_unique<juce::DynamicObject>();settings->setProperty("punch",record.punch);settings->setProperty("loop",record.loop);settings->setProperty("punchIn",record.punchIn);settings->setProperty("punchOut",record.punchOut);settings->setProperty("preRoll",record.preRoll);settings->setProperty("postRoll",record.postRoll);settings->setProperty("loopStart",record.loopStart);settings->setProperty("loopEnd",record.loopEnd);root->setProperty("recording",juce::var(settings.release()));
    root->setProperty("masterGain", engine.masterGain.load());
    root->setProperty("masterMute",engine.masterMute.load());root->setProperty("masterInserts",insertState(engine.masterInserts));
    root->setProperty("bpm",engine.bpm.load()); root->setProperty("beatsPerBar",engine.beatsPerBar); root->setProperty("beatUnit",engine.beatUnit);
    root->setProperty("loopEnabled",engine.loopEnabled.load()); root->setProperty("loopStart",engine.loopStart.load()); root->setProperty("loopEnd",engine.loopEnd.load());
    juce::Array<juce::var> rows;
    for (int i = 0; i < engine.trackCount(); ++i) {
        const auto& t = engine.track(i);
        auto row = std::make_unique<juce::DynamicObject>();
        row->setProperty("emptyAudio",!t.auxiliary&&!t.midiTrack&&t.regions.empty()&&t.clip->source==juce::File{});
        row->setProperty("recordArm",t.recordArm.load());row->setProperty("monitorPreference",t.monitorPreference);row->setProperty("recordGain",t.recordGain.load());row->setProperty("recordInputLeft",t.recordInputLeft);row->setProperty("recordInputRight",t.recordInputRight);row->setProperty("recordStereo",t.recordStereo);row->setProperty("activeTake",int64_t(t.activeTake));
        juce::Array<juce::var> takes;for(const auto& take:t.takes){auto o=std::make_unique<juce::DynamicObject>();o->setProperty("id",int64_t(take.id));o->setProperty("group",int64_t(take.group));o->setProperty("file",take.file.getRelativePathFrom(file.getParentDirectory()));o->setProperty("start",take.start);o->setProperty("length",take.length);o->setProperty("rate",take.rate);o->setProperty("channels",take.channels);takes.add(juce::var(o.release()));}row->setProperty("takes",takes);
        row->setProperty("midiTrack",t.midiTrack);row->setProperty("midiInput",t.midiInput);row->setProperty("midiArm",t.midiArm.load());row->setProperty("instrument",t.instrument?t.instrument->state():juce::var());
        juce::Array<juce::var> midiClips;for(const auto& clip:t.midiClips)midiClips.add(midiClipState(clip));row->setProperty("midiClips",midiClips);
        row->setProperty("auxiliary",t.auxiliary);row->setProperty("monoBus",t.monoBus);row->setProperty("mix",mixState(t));
        row->setProperty("file", t.regions.empty()&&!t.midiTrack&&!t.auxiliary&&t.clip->source==juce::File{}?juce::String("@empty"):t.midiTrack?juce::String("@midi"):t.auxiliary ? juce::String("@aux"):t.clip->source.getRelativePathFrom(file.getParentDirectory()));
        row->setProperty("name", t.name); row->setProperty("gain", t.gain.load());
        row->setProperty("pan", t.pan.load()); row->setProperty("mute", t.mute.load()); row->setProperty("solo", t.solo.load());
        row->setProperty("colour",juce::String::toHexString(static_cast<int64_t>(t.colour)));
        row->setProperty("height",t.height); row->setProperty("outputMode",t.outputMode.load());
        juce::Array<juce::var> regions;
        for(const auto& r:t.regions) {
            auto item=std::make_unique<juce::DynamicObject>();
            item->setProperty("file",r.asset->source.getRelativePathFrom(file.getParentDirectory()));
            item->setProperty("fadeIn",r.fadeIn);item->setProperty("fadeOut",r.fadeOut);item->setProperty("fadeInCurve",int(r.fadeInCurve));item->setProperty("fadeOutCurve",int(r.fadeOutCurve));
            item->setProperty("original",r.original?r.original->source.getRelativePathFrom(file.getParentDirectory()):juce::String{});item->setProperty("originalOffset",r.originalOffset);item->setProperty("originalLength",r.originalLength);item->setProperty("stretch",r.stretch);item->setProperty("semitones",r.semitones);item->setProperty("cents",r.cents);
            item->setProperty("takeId",int64_t(r.takeId));item->setProperty("start",r.start); item->setProperty("offset",r.offset); item->setProperty("length",r.length);
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
    if(file.getSize()>64LL*1024*1024){error="Sessao excede o limite de 64 MiB de metadados.";return false;}
    juce::var data;
    if (juce::JSON::parse(file.loadFileAsString(), data).failed() || !data.isObject()
        || data["format"].toString() != "nova-daw-session" || !data["version"].isInt() || (int(data["version"])<1 || int(data["version"])>7)
        || !data["tracks"].isArray() || !numeric(data["masterGain"])) { error = "Sessao invalida ou versao nao suportada."; return false; }
    auto* rows = data["tracks"].getArray();
    if (rows->size() > AudioEngine::maxTracks) { error = "Sessao excede 64 faixas."; return false; }
    const bool v2=int(data["version"])>=2,v3=int(data["version"])>=3,v4=int(data["version"])>=4,v5=int(data["version"])>=5,v6=int(data["version"])>=6;
    if(v4&&(!data["midiPpq"].isInt()||int(data["midiPpq"])!=midiPpq||!data["metronome"].isBool())){error="Relogio MIDI/metronomo invalido.";return false;}
    if(v2 && (!numeric(data["bpm"]) || !data["beatsPerBar"].isInt() || !data["beatUnit"].isInt()
        || !numeric(data["loopStart"]) || !numeric(data["loopEnd"]) || !data["loopEnabled"].isBool())) {
        error="Dados de tempo, compasso ou loop invalidos.";return false;
    }
    RecordingOptions recording;if(int(data["version"])>=5){const auto& v=data["recording"];if(!v.isObject()||!v["punch"].isBool()||!v["loop"].isBool()){error="Configuracao de gravacao invalida.";return false;}
        for(const auto* key:{"punchIn","punchOut","preRoll","postRoll","loopStart","loopEnd"})if(!numeric(v[key])||!std::isfinite(double(v[key]))){error="Tempo de gravacao invalido.";return false;}
        recording.punch=bool(v["punch"]);recording.loop=bool(v["loop"]);recording.punchIn=double(v["punchIn"]);recording.punchOut=double(v["punchOut"]);recording.preRoll=double(v["preRoll"]);recording.postRoll=double(v["postRoll"]);recording.loopStart=double(v["loopStart"]);recording.loopEnd=double(v["loopEnd"]);
        if(recording.punchIn<0||recording.punchOut<=recording.punchIn||recording.punchOut>86400||recording.preRoll<0||recording.preRoll>60||recording.postRoll<0||recording.postRoll>60||recording.loopStart<0||recording.loopEnd<=recording.loopStart||recording.loopEnd>86400){error="Intervalos de gravacao invalidos.";return false;}}
    struct Entry { std::shared_ptr<Clip> clip; float gain, pan; bool mute, solo; juce::String name; uint32_t colour; int height, route; std::vector<Track::Region> regions; bool midi=false;std::vector<MidiClip> midiClips; };
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
        std::shared_ptr<Clip> clip;
        if(v4&&(!row["midiTrack"].isBool()||!row["midiArm"].isBool()||!row["midiInput"].isString()||row["midiInput"].toString().length()>256||!row["midiClips"].isArray()||row["midiClips"].size()>128)){error="Tipo/entrada MIDI invalida.";return false;}
        if(v3 && (!row["auxiliary"].isBool() || !row["monoBus"].isBool())){error="Tipo de canal invalido.";return false;}
        if(v3 && (bool(row["auxiliary"])||(v4&&bool(row["midiTrack"]))||(v5&&bool(row["emptyAudio"])))) {clip=std::make_shared<Clip>();clip->name=row["name"].toString();clip->samples.setSize(2,1);clip->samples.clear();}
        else clip=loadAsset(row["file"]);
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
                uint64_t takeId=0;if(v5){if(!(item["takeId"].isInt()||item["takeId"].isInt64())||int64_t(item["takeId"])<0){error="Referencia de take invalida.";return false;}takeId=uint64_t(int64_t(item["takeId"]));}
                Track::Region region{0,asset,start,offset,length,takeId};
                if(v6){for(const auto* key:{"fadeIn","fadeOut","originalOffset","originalLength","stretch","semitones","cents"})if(!numeric(item[key])||!std::isfinite(double(item[key]))){error="Edicao de audio invalida.";return false;}
                    if(!item["fadeInCurve"].isInt()||!item["fadeOutCurve"].isInt()||int(item["fadeInCurve"])<0||int(item["fadeInCurve"])>2||int(item["fadeOutCurve"])<0||int(item["fadeOutCurve"])>2||!item["original"].isString()){error="Curva/referencia original invalida.";return false;}
                    region.fadeIn=double(item["fadeIn"]);region.fadeOut=double(item["fadeOut"]);region.fadeInCurve=FadeCurve(int(item["fadeInCurve"]));region.fadeOutCurve=FadeCurve(int(item["fadeOutCurve"]));region.originalOffset=double(item["originalOffset"]);region.originalLength=double(item["originalLength"]);region.stretch=double(item["stretch"]);region.semitones=double(item["semitones"]);region.cents=double(item["cents"]);
                    if(region.fadeIn<0||region.fadeOut<0||region.fadeIn>length||region.fadeOut>length||region.stretch<0.25||region.stretch>4||std::abs(region.semitones)>12||std::abs(region.cents)>100||region.originalOffset<0||region.originalLength<0){error="Fades/transformacao fora dos limites.";return false;}
                    if(item["original"].toString().isNotEmpty()){region.original=loadAsset(item["original"]);if(!region.original)return false;if(region.originalLength<=0||region.originalOffset+region.originalLength>region.original->duration()+1e-8){error="Fonte original fora dos limites.";return false;}}
                    else if(region.originalOffset!=0||region.originalLength!=0||region.stretch!=1||region.semitones!=0||region.cents!=0){error="Transformacao sem fonte original.";return false;}
                }
                entry.regions.push_back(std::move(region));
            }
        }else entry.regions.push_back({0,clip,0,0,clip->duration()});
        if(v4){entry.midi=bool(row["midiTrack"]);for(const auto& item:*row["midiClips"].getArray()){MidiClip midi;if(!readMidiClip(item,midi,error))return false;
                if(ticksToSeconds(midi.start+midi.length,bpm)>86400){error="Clip MIDI excede timeline de 24 h.";return false;}entry.midiClips.push_back(std::move(midi));}
            if((entry.midi&&bool(row["auxiliary"]))||(!entry.midi&&(!entry.midiClips.empty()||!row["instrument"].isVoid()))||(entry.midi&&!entry.regions.empty())){error="Dados MIDI nao correspondem ao tipo de faixa.";return false;}}
        entries.push_back(std::move(entry));
    }
    double end=0;for(const auto& entry:entries){for(const auto& r:entry.regions)end=std::max(end,r.end());for(const auto& midi:entry.midiClips)end=std::max(end,ticksToSeconds(midi.start+midi.length,bpm));}
    if(v2 && static_cast<bool>(data["loopEnabled"]) && (le<=ls || le>end+1e-8)){error="Loop fora da sessao.";return false;}
    auto candidate=std::make_unique<AudioEngine>();candidate->setOutputRate(engine.sampleRate());
    for (auto& entry : entries) {
        if(!candidate->addSharedClip(entry.clip, error))return false;
        auto& t = candidate->track(candidate->trackCount() - 1);
        t.gain = entry.gain; t.pan = entry.pan; t.mute = entry.mute; t.solo = entry.solo;
        t.name=entry.name;t.colour=entry.colour;t.height=entry.height;t.outputMode=entry.route;
        t.regions=std::move(entry.regions);for(auto& r:t.regions)r.id=candidate->newRegionId();
        t.midiTrack=entry.midi;t.midiClips=std::move(entry.midiClips);for(auto& midi:t.midiClips){midi.id=candidate->newRegionId();for(auto& note:midi.notes)note.id=candidate->newRegionId();}
        if(t.midiTrack&&!candidate->rebuildMidi(candidate->trackCount()-1,error))return false;
    }
    candidate->recalculateMediaBytes();candidate->bpm=bpm;candidate->beatsPerBar=beats;candidate->beatUnit=unit;
    candidate->setLoop(ls,le,v2 && static_cast<bool>(data["loopEnabled"]));
    candidate->recordingOptions=recording;candidate->masterGain=master;candidate->metronomeEnabled=v4&&bool(data["metronome"]);
    if(v3){if(!data["masterMute"].isBool() || !readInserts(data["masterInserts"],candidate->masterInserts,engine.sampleRate(),error))return false;
        candidate->masterMute=bool(data["masterMute"]);
        for(int i=0;i<rows->size();++i){auto& t=candidate->track(i);const auto& row=rows->getReference(i);t.auxiliary=bool(row["auxiliary"]);t.monoBus=bool(row["monoBus"]);
            if(t.auxiliary && !t.regions.empty()){error="Aux nao pode conter regioes WAV.";return false;}
            if(!readMix(row["mix"],t,engine.sampleRate(),error))return false;
            if(v4){t.midiInput=row["midiInput"].toString();t.midiArm=bool(row["midiArm"]);if(!row["instrument"].isVoid()){
                t.instrument=MixProcessor::restore(row["instrument"],engine.sampleRate(),error);if(!t.instrument||!t.instrument->isInstrument()){error="Slot de instrumento MIDI invalido.";return false;}}}}
        if(v5)for(int i=0;i<rows->size();++i){auto& t=candidate->track(i);const auto& row=rows->getReference(i);
            auto integer=[](const juce::var& value){return value.isInt()||value.isInt64();};
            if(!row["emptyAudio"].isBool()||!row["recordArm"].isBool()||!row["monitorPreference"].isBool()||!row["recordStereo"].isBool()||!row["recordInputLeft"].isInt()||!row["recordInputRight"].isInt()||!numeric(row["recordGain"])||!integer(row["activeTake"])||!row["takes"].isArray()||row["takes"].size()>128){error="Configuracao de gravacao invalida.";return false;}
            t.recordInputLeft=int(row["recordInputLeft"]);t.recordInputRight=int(row["recordInputRight"]);t.recordGain=float(row["recordGain"]);t.recordStereo=bool(row["recordStereo"]);t.recordArm=bool(row["recordArm"]);t.monitorPreference=bool(row["monitorPreference"]);t.monitorInput=false;t.activeTake=uint64_t(int64_t(row["activeTake"]));
            if(t.recordInputLeft<0||t.recordInputLeft>63||t.recordInputRight<0||t.recordInputRight>63||!std::isfinite(t.recordGain.load())||t.recordGain<0||t.recordGain>16||int64_t(row["activeTake"])<0){error="Entradas/ganho de gravacao fora dos limites.";return false;}
            std::set<uint64_t> ids;for(const auto& item:*row["takes"].getArray()){
                if(!item.isObject()||!integer(item["id"])||!integer(item["group"])||!item["file"].isString()||!numeric(item["start"])||!numeric(item["length"])||!numeric(item["rate"])||!item["channels"].isInt()){error="Take invalido.";return false;}
                Track::Take take;take.id=uint64_t(int64_t(item["id"]));take.group=uint64_t(int64_t(item["group"]));take.file=file.getParentDirectory().getChildFile(item["file"].toString());take.start=double(item["start"]);take.length=double(item["length"]);take.rate=double(item["rate"]);take.channels=int(item["channels"]);
                if(int64_t(item["id"])<=0||int64_t(item["id"])>1000000000000LL||int64_t(item["group"])<=0||int64_t(item["group"])>1000000000000LL||!ids.insert(take.id).second||!take.file.existsAsFile()||!std::isfinite(take.start)||!std::isfinite(take.length)||!std::isfinite(take.rate)||take.start<0||take.length<=0||take.start+take.length>86400||take.rate<100||take.rate>768000||(take.channels!=1&&take.channels!=2)){error="Take gravado ausente ou fora dos limites.";return false;}candidate->reserveRegionIds(take.id);candidate->reserveRegionIds(take.group);t.takes.push_back(std::move(take));}
            if(t.activeTake!=0&&!ids.contains(t.activeTake)){error="Take ativo inexistente.";return false;}for(const auto& r:t.regions)if(r.takeId!=0&&!ids.contains(r.takeId)){error="Regiao aponta para take inexistente.";return false;}
        }
        if(!candidate->rebuildGraph(error))return false;
    }
    engine.replaceSession(*candidate);return true;
}
}
