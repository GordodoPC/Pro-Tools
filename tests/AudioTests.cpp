#include "Audio/AudioEngine.h"
#include "Session/Session.h"
#include "Session/EditModel.h"
#include "Session/Preferences.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void expect(bool condition, const char* label) {
    ++checks;
    if (!condition) throw std::runtime_error(label);
    std::cout << "PASS: " << label << '\n';
}
bool near(float actual, float expected) { return std::abs(actual - expected) < 0.00002f; }
void writeWave(const juce::File& file, double rate, float left, float right, bool tone = false, bool mono = false) {
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    expect(stream != nullptr, "create WAV output");
    stream->setPosition(0); stream->truncate();
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), rate, mono ? 1u : 2u, 24, {}, 0));
    expect(writer != nullptr, "create WAV writer");
    juce::AudioBuffer<float> samples(mono ? 1 : 2, static_cast<int>(rate * 5));
    for (int i = 0; i < samples.getNumSamples(); ++i) {
        samples.setSample(0,i, tone ? left * static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi * 220 * i / rate)) : left);
        if (!mono) samples.setSample(1,i, tone ? right * static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi * 330 * i / rate)) : right);
    }
    expect(writer->writeFromAudioSampleBuffer(samples,0,samples.getNumSamples()), "write real WAV samples");
}
void add(nova::AudioEngine& engine, const juce::File& file) {
    juce::String error;
    auto clip = nova::AudioEngine::loadWav(file,error);
    expect(clip != nullptr, "decode WAV");
    expect(!clip->waveform.empty(), "waveform generated from real samples");
    expect(engine.addClip(std::move(clip),error), "add track");
}
juce::AudioBuffer<float> render(nova::AudioEngine& engine, int frames = 128) {
    juce::AudioBuffer<float> output(2,frames);
    engine.render(output.getArrayOfWritePointers(),2,frames);
    return output;
}
}
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser;
    try {
        expect(argc > 1,"test output directory supplied");
        const juce::File dir(juce::String::fromUTF8(argv[1])); dir.createDirectory();
        const auto a = dir.getChildFile("track-a.wav"), b = dir.getChildFile("track-b.wav");
        writeWave(a,48000,0.1f,0.2f); writeWave(b,44100,0.05f,-0.1f);
        nova::AudioEngine engine; engine.setOutputRate(48000); add(engine,a); add(engine,b);
        expect(engine.trackCount()==2,"two imported tracks");
        expect(near(engine.track(0).clip->waveform.front().high,0.2f),"waveform peak matches decoded WAV");
        engine.play(); auto out = render(engine);
        expect(near(out.getSample(0,0),0.15f) && near(out.getSample(1,0),0.1f),"simultaneous stereo sum and mixed sample rates");
        expect(engine.position()>0,"transport advances");
        engine.track(1).gain=0.5f; out=render(engine);
        expect(near(out.getSample(0,0),0.125f),"track gain changes audio");
        engine.track(1).mute=true; out=render(engine);
        expect(near(out.getSample(0,0),0.1f),"mute excludes track");
        engine.track(1).mute=false; engine.track(1).gain=1; engine.track(1).solo=true; out=render(engine);
        expect(near(out.getSample(0,0),0.05f),"solo excludes other tracks");
        engine.track(1).mute=true; out=render(engine);
        expect(out.getMagnitude(0,out.getNumSamples())==0,"mute takes precedence over solo");
        engine.track(1).mute=false; engine.track(1).pan=1; out=render(engine);
        expect(near(out.getSample(0,0),0) && near(out.getSample(1,0),-0.1f),"stereo pan to right");
        engine.masterGain=0.5f; out=render(engine);
        expect(near(out.getSample(1,0),-0.05f),"master gain changes audio");
        engine.pause(); const auto paused=engine.position(); out=render(engine);
        expect(engine.position()==paused && out.getMagnitude(0,out.getNumSamples())==0,"pause preserves position and silences output");
        engine.play(); out=render(engine); expect(engine.position()>paused,"play resumes after pause");
        engine.seek(1); out=render(engine); expect(engine.position()>1 && engine.position()<1.1,"seek uses requested time");
        engine.stop(); out=render(engine); expect(engine.position()==0 && out.getMagnitude(0,out.getNumSamples())==0,"stop rewinds and silences output");
        engine.seek(4.999); engine.play(); out=render(engine); expect(!engine.isPlaying() && engine.position()==5,"end of session stops transport");
        juce::String error;
        const auto session=dir.getChildFile("roundtrip.novadaw");
        expect(nova::Session::save(session,engine,error),"save session");
        nova::AudioEngine restored;
        expect(nova::Session::load(session,restored,error),"open session");
        expect(restored.trackCount()==2 && restored.track(1).solo && near(restored.track(1).pan,1)
            && near(restored.masterGain,0.5f),"session persists files and mix controls");
        const auto bad=dir.getChildFile("invalid.novadaw"); bad.replaceWithText("{}");
        expect(!nova::Session::load(bad,restored,error) && restored.trackCount()==2,"invalid session leaves previous session intact");
        const auto missing=dir.getChildFile("missing.novadaw");
        missing.replaceWithText("{\"format\":\"nova-daw-session\",\"version\":1,\"masterGain\":1,\"tracks\":[{\"file\":\"missing.wav\",\"gain\":1,\"pan\":0,\"mute\":false,\"solo\":false}]}");
        expect(!nova::Session::load(missing,restored,error) && restored.trackCount()==2,"missing audio leaves previous session intact");
        auto invalidControls=juce::JSON::parse(session.loadFileAsString());
        invalidControls["tracks"].getArray()->getReference(0).getDynamicObject()->setProperty("gain", "invalid");
        bad.replaceWithText(juce::JSON::toString(invalidControls));
        expect(!nova::Session::load(bad,restored,error) && restored.trackCount()==2,"reject malformed mix controls transactionally");
        const auto corrupt=dir.getChildFile("corrupt.wav"); corrupt.replaceWithText("not audio");
        expect(nova::AudioEngine::loadWav(corrupt,error)==nullptr,"reject corrupt WAV");
        const auto mono=dir.getChildFile("mono.wav"); writeWave(mono,48000,0.1f,0,true,true);
        nova::AudioEngine monoEngine; add(monoEngine,mono); monoEngine.setOutputRate(48000); monoEngine.track(0).pan=-1; monoEngine.play();
        out=render(monoEngine); expect(out.getMagnitude(1,0,out.getNumSamples())==0,"mono pan routes to left only");
        const auto toneA=dir.getChildFile("demo-tone-a.wav"),toneB=dir.getChildFile("demo-tone-b.wav");
        writeWave(toneA,48000,0.08f,0.08f,true); writeWave(toneB,44100,0.04f,0.05f,true);
        nova::AudioEngine demo; add(demo,toneA); add(demo,toneB);
        demo.setOutputRate(48000); demo.play(); out=render(demo);
        const float expected = 0.12f * static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi * 220 * 17 / 48000.0));
        expect(std::abs(out.getSample(0,17)-expected)<0.0001f,"resampling 44.1 kHz sine into 48 kHz output");
        demo.stop();
        expect(nova::Session::save(dir.getChildFile("demo.novadaw"),demo,error),"save real audio fixture for native UI validation");
        nova::AudioEngine capacity;
        for(int i=0;i<nova::AudioEngine::maxTracks;++i) {
            auto clip=std::make_unique<nova::Clip>(); clip->sampleRate=48000; clip->samples.setSize(2,8); clip->samples.clear();
            if(!capacity.addClip(std::move(clip),error)) throw std::runtime_error("track capacity below 64");
        }
        auto extra=std::make_unique<nova::Clip>(); extra->samples.setSize(2,8);
        expect(!capacity.addClip(std::move(extra),error) && capacity.trackCount()==64,"enforce 64 track limit");
        for(int i=0;i<capacity.trackCount();++i) for(int c=0;c<2;++c) for(int s=0;s<8;++s) capacity.track(i).clip->samples.setSample(c,s,0.1f);
        capacity.setOutputRate(48000); capacity.play(); out=render(capacity,4);
        expect(near(out.getSample(0,0),1) && capacity.masterPeak.load()>1,"master overload indication and output hard clipping protection");
        juce::MemoryBlock original; a.loadFileAsData(original);
        nova::AudioEngine edited; edited.setOutputRate(48000); add(edited,a); add(edited,b);
        edited.track(1).regions.clear(); nova::EditModel edit(edited);
        const auto id=edited.track(0).regions.front().id; edit.select(0,id);
        expect(std::abs(edit.quantize(0.26)-0.5)<1e-9,"musical snap at 120 BPM");
        expect(edit.move(2,0),"move clip metadata"); edited.play(); out=render(edited);
        expect(out.getMagnitude(0,out.getNumSamples())==0,"moved clip starts with silence");
        edited.seek(2);out=render(edited);expect(near(out.getSample(0,0),0.1f),"moved clip renders at new position");
        expect(edit.split(3) && edited.track(0).regions.size()==2,"split creates two source references");
        edited.seek(3);out=render(edited);expect(near(out.getSample(0,0),0.1f),"split boundary has no doubled sample");
        edit.select(0,id);expect(edit.trim(2.25,2.75),"trim updates offset and duration");
        edited.seek(2);out=render(edited);expect(out.getMagnitude(0,out.getNumSamples())==0,"trimmed-away interval is silent");
        expect(edit.copy() && edit.paste(1,4),"copy/paste across tracks");
        expect(edited.track(1).regions.front().asset==edited.track(0).clip,"paste shares audio without copying or altering media");
        expect(edit.undo() && edited.track(1).regions.empty(),"undo paste");
        expect(edit.redo() && edited.track(1).regions.size()==1,"redo paste");
        expect(edit.erase() && edited.track(1).regions.empty(),"delete region");
        expect(edit.undo() && edited.track(1).regions.size()==1,"undo delete restores asset reference");
        edit.rename(0,"Vocal edited");edit.recolour(0,0xffee7799);edit.resizeTrack(0,200);edited.track(1).outputMode=2;
        edited.bpm=96;edited.beatsPerBar=3;edited.beatUnit=4;
        expect(nova::Session::save(dir.getChildFile("edited.novadaw"),edited,error),"save v2 edited session");
        nova::AudioEngine editedLoaded;
        expect(nova::Session::load(dir.getChildFile("edited.novadaw"),editedLoaded,error),"open v2 edited session");
        expect(editedLoaded.track(0).regions.size()==2 && editedLoaded.track(0).name=="Vocal edited"
            && editedLoaded.track(0).colour==0xffee7799 && editedLoaded.track(0).height==200
            && editedLoaded.track(1).outputMode==2 && editedLoaded.bpm==96 && editedLoaded.beatsPerBar==3,
            "edited session persists regions, names, colours, heights, routing and tempo");
        juce::MemoryBlock after; a.loadFileAsData(after);expect(after==original,"all edits preserve original WAV bytes");
        nova::AudioEngine loop;loop.setOutputRate(1000);add(loop,a);loop.setLoop(0.5,1,true);loop.seek(0.9);loop.play();
        out=render(loop,250);expect(loop.isPlaying() && std::abs(loop.position()-0.65)<1e-7,"sample-time loop wraps inside a callback");
        expect(near(out.getSample(0,100),0.1f),"loop renders audio across wrap boundary");
        loop.loopEnabled=false;loop.track(0).outputMode=2;out=render(loop);
        expect(out.getMagnitude(0,0,out.getNumSamples())==0 && near(out.getSample(1,0),0.15f),"track output routes to master right");
        const auto legacy=dir.getChildFile("legacy.novadaw");
        legacy.replaceWithText("{\"format\":\"nova-daw-session\",\"version\":1,\"masterGain\":1,\"tracks\":[{\"file\":\"track-a.wav\",\"gain\":1,\"pan\":0,\"mute\":false,\"solo\":false}]}");
        expect(nova::Session::load(legacy,editedLoaded,error) && editedLoaded.track(0).regions.size()==1,"phase 1 sessions remain readable");
        edit.select(0,id);expect(!edit.move(86400,0) && edit.copy() && !edit.paste(1,86400),"edits respect the session's 24 hour limit");
        nova::EditModel toneEdit(demo); toneEdit.select(0,demo.track(0).regions.front().id);
        demo.seek(1.125);demo.play();const auto reference=render(demo);
        expect(toneEdit.split(1),"split a real tone clip");demo.seek(1.125);out=render(demo);
        bool same=true;for(int c=0;c<2;++c)for(int s=0;s<out.getNumSamples();++s)same &= near(out.getSample(c,s),reference.getSample(c,s));
        expect(same,"split preserves source sample phase and stereo mix");
        auto malformed=juce::JSON::parse(session.loadFileAsString());malformed.getDynamicObject()->setProperty("loopEnabled","yes");
        bad.replaceWithText(juce::JSON::toString(malformed));
        expect(!nova::Session::load(bad,restored,error) && restored.trackCount()==2,"reject malformed loop transactionally");
        malformed=juce::JSON::parse(session.loadFileAsString());malformed["tracks"].getArray()->getReference(0).getDynamicObject()->setProperty("colour","bad-colour");
        bad.replaceWithText(juce::JSON::toString(malformed));
        expect(!nova::Session::load(bad,restored,error) && restored.trackCount()==2,"reject malformed track colour transactionally");
        nova::Preferences preferences;preferences.trackWidth=310;preferences.clipWidth=230;preferences.horizontalZoom=240;preferences.verticalZoom=1.5;preferences.mix=true;preferences.snap=false;
        const auto preferencesFile=dir.getChildFile("preferences.json");expect(preferences.save(preferencesFile),"save editor preferences");
        nova::Preferences loadedPreferences;loadedPreferences.load(preferencesFile);
        expect(loadedPreferences.trackWidth==310 && loadedPreferences.clipWidth==230 && loadedPreferences.horizontalZoom==240 && loadedPreferences.verticalZoom==1.5 && loadedPreferences.mix && !loadedPreferences.snap,"preferences restore pane sizes, zoom, view and snap");
        preferencesFile.replaceWithText("{\"version\":1,\"horizontalZoom\":10000}");loadedPreferences=nova::Preferences{};loadedPreferences.load(preferencesFile);
        expect(loadedPreferences.horizontalZoom==600 && loadedPreferences.trackWidth==250 && loadedPreferences.clipWidth==190 && loadedPreferences.verticalZoom==1 && loadedPreferences.snap,"partial preferences keep defaults and clamp invalid sizes");
        std::cout << checks << " checks passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
