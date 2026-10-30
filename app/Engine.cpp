#include "Engine.h"
#include <cmath>

using namespace giro;

// ---------------------------------------------------------------- SimpleSynth

void SimpleSynth::prepare(double sampleRate, int) {
    sr = sampleRate;
    for (auto& v : voices) v.active = false;
    for (auto& c : chanVol) c = 1.0f;
}

void SimpleSynth::noteOn(int ch, int note, int vel) {
    Voice* v = nullptr;
    for (auto& x : voices) if (!x.active) { v = &x; break; }
    if (!v) { v = &voices[0]; for (auto& x : voices) if (x.env < v->env) v = &x; }
    *v = Voice();
    v->active = true; v->ch = ch; v->note = note; v->held = true;
    float g = vel / 127.0f;
    if (ch == 10) {   // drums
        v->kind = 3;
        switch (note) {
        case GM::Kick:    v->noise = false; v->inc = 150.0 / sr; v->pitchDrop = std::pow(0.3, 1.0 / (0.12 * sr)); v->decay = (float)std::exp(-1.0 / (0.25 * sr)); v->gain = 0.9f * g; break;
        case GM::Snare:   v->noise = true; v->lpCoef = 0.35f; v->decay = (float)std::exp(-1.0 / (0.14 * sr)); v->gain = 0.5f * g; break;
        case GM::Rim:     v->noise = true; v->lpCoef = 0.6f; v->decay = (float)std::exp(-1.0 / (0.03 * sr)); v->gain = 0.6f * g; break;
        case GM::Ride:    v->noise = true; v->lpCoef = 0.9f; v->decay = (float)std::exp(-1.0 / (0.45 * sr)); v->gain = 0.25f * g; break;
        case GM::HatOpen: v->noise = true; v->lpCoef = 0.95f; v->decay = (float)std::exp(-1.0 / (0.25 * sr)); v->gain = 0.22f * g; break;
        case GM::ClickHi: case GM::ClickLo: v->noise = false; v->inc = (note == GM::ClickHi ? 1500.0 : 1000.0) / sr; v->decay = (float)std::exp(-1.0 / (0.05 * sr)); v->gain = 0.6f * g; break;
        default:          v->noise = true; v->lpCoef = 0.97f; v->decay = (float)std::exp(-1.0 / (0.05 * sr)); v->gain = 0.22f * g; break;   // closed hat, pedal
        }
    } else {
        v->kind = ch == 1 ? 1 : 2;
        v->inc = 440.0 * std::pow(2.0, (note - 69) / 12.0) / sr;
        v->decay = (float)std::exp(-1.0 / ((ch == 1 ? 0.9 : 0.6) * sr));
        v->gain = (ch == 1 ? 0.5f : 0.16f) * (0.4f + 0.6f * g);
        v->lpCoef = ch == 1 ? 0.08f : 0.35f;
    }
    v->env = 1.0f;
}

void SimpleSynth::noteOff(int ch, int note) {
    for (auto& v : voices) if (v.active && v.ch == ch && v.note == note && v.held) { v.held = false; v.decay = (float)std::exp(-1.0 / (0.03 * sr)); }
}

void SimpleSynth::process(juce::AudioBuffer<float>& out, juce::MidiBuffer& midi) {
    const int n = out.getNumSamples();
    out.clear();
    auto it = midi.begin();
    for (int s = 0; s < n; s++) {
        while (it != midi.end() && (*it).samplePosition <= s) {
            auto m = (*it).getMessage();
            if (m.isNoteOn()) noteOn(m.getChannel(), m.getNoteNumber(), m.getVelocity());
            else if (m.isNoteOff()) noteOff(m.getChannel(), m.getNoteNumber());
            else if (m.isController() && m.getControllerNumber() == 7) chanVol[m.getChannel() - 1] = m.getControllerValue() / 127.0f;
            ++it;
        }
        float mix = 0;
        for (auto& v : voices) {
            if (!v.active) continue;
            float x;
            if (v.noise) x = rng.nextFloat() * 2 - 1;
            else {
                v.phase += v.inc; if (v.phase >= 1) v.phase -= 1;
                if (v.kind == 1) x = (float)(2 * v.phase - 1) * 0.6f + (float)std::sin(v.phase * juce::MathConstants<double>::twoPi) * 0.6f;   // bass: saw + sine
                else if (v.kind == 2) x = (float)std::sin(v.phase * juce::MathConstants<double>::twoPi) + 0.25f * (float)std::sin(2 * v.phase * juce::MathConstants<double>::twoPi);  // EP-ish
                else x = (float)std::sin(v.phase * juce::MathConstants<double>::twoPi);
                if (v.pitchDrop > 0) v.inc *= v.pitchDrop;
            }
            v.lp += v.lpCoef * (x - v.lp);
            float y = (v.kind == 3 && v.noise) ? (x - v.lp) : v.lp;   // drums noise: high-passed
            if (v.kind == 3 && v.noise && v.note != GM::Snare) y = x - v.lp;
            mix += y * v.env * v.gain * chanVol[v.ch - 1];
            v.env *= v.decay;
            if (v.env < 0.0005f) v.active = false;
        }
        mix = std::tanh(mix * 1.2f);
        for (int c = 0; c < out.getNumChannels(); c++) out.setSample(c, s, mix);
    }
}

// ---------------------------------------------------------------- AppleGMSynth

#if JUCE_MAC
std::unique_ptr<AppleGMSynth> AppleGMSynth::tryCreate(double sampleRate, int blockSize) {
    juce::AudioPluginFormatManager fm;
    fm.addDefaultFormats();
    for (auto* format : fm.getFormats()) {
        if (format->getName() != "AudioUnit") continue;
        juce::OwnedArray<juce::PluginDescription> found;
        format->findAllTypesForFile(found, "AudioUnit:Synths/aumu,dls ,appl");
        if (found.isEmpty()) return nullptr;
        juce::String err;
        auto inst = fm.createPluginInstance(*found[0], sampleRate, blockSize, err);
        if (inst == nullptr) return nullptr;
        auto s = std::unique_ptr<AppleGMSynth>(new AppleGMSynth());
        s->au = std::move(inst);
        s->prepare(sampleRate, blockSize);
        return s;
    }
    return nullptr;
}

void AppleGMSynth::prepare(double sampleRate, int blockSize) {
    if (au == nullptr) return;
    au->enableAllBuses();
    au->setPlayConfigDetails(0, 2, sampleRate, blockSize);
    au->prepareToPlay(sampleRate, blockSize);
    scratch.setSize(juce::jmax(2, au->getTotalNumOutputChannels()), blockSize);
}

void AppleGMSynth::process(juce::AudioBuffer<float>& out, juce::MidiBuffer& midi) {
    if (au == nullptr) { out.clear(); return; }
    if (scratch.getNumSamples() < out.getNumSamples()) scratch.setSize(scratch.getNumChannels(), out.getNumSamples(), false, false, true);
    juce::AudioBuffer<float> block(scratch.getArrayOfWritePointers(), scratch.getNumChannels(), out.getNumSamples());
    block.clear();
    au->processBlock(block, midi);
    for (int c = 0; c < out.getNumChannels(); c++) out.copyFrom(c, 0, block, juce::jmin(c, block.getNumChannels() - 1), 0, out.getNumSamples());
}
#endif

// ---------------------------------------------------------------- Engine

Engine::Engine() {}

void Engine::prepareToPlay(int blockSize, double sampleRate) {
    sr = sampleRate;
#if JUCE_MAC
    if (synth == nullptr) synth = AppleGMSynth::tryCreate(sampleRate, blockSize);
#endif
    if (synth == nullptr) synth = std::make_unique<SimpleSynth>();
    synth->prepare(sampleRate, blockSize);
    volDirty = true;
}

void Engine::releaseResources() {}

juce::String Engine::backendName() const { return synth ? synth->name() : juce::String("—"); }

void Engine::start(Sequence seq, double bpm, int bassProgram, int compProgram) {
    stop();
    {
        const juce::ScopedLock sl(seqLock);
        sequence = std::move(seq);
        nextEvent = 0; offs.clear();
    }
    tempo = bpm; beatPos = 0; done = false;
    programs[0] = bassProgram; programs[1] = compProgram;
    startRequested = true;
    playing = true;
}

void Engine::stop() {
    if (playing.exchange(false)) allNotesOff = true;
}

void Engine::setVolume(Part part, float v) {
    vol[part == Part::Bass ? 0 : (part == Part::Comp ? 1 : 2)] = v;
    volDirty = true;
}

void Engine::getNextAudioBlock(const juce::AudioSourceChannelInfo& info) {
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> out(info.buffer->getArrayOfWritePointers(), info.buffer->getNumChannels(), info.startSample, info.numSamples);
    if (synth == nullptr) { out.clear(); return; }

    if (allNotesOff.exchange(false)) {
        for (int ch : {1, 2, 10}) midi.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
        for (int ch : {1, 2}) for (int n = 0; n < 128; n++) midi.addEvent(juce::MidiMessage::noteOff(ch, n), 0);
        const juce::ScopedLock sl(seqLock); offs.clear();
    }
    if (startRequested.exchange(false)) {
        midi.addEvent(juce::MidiMessage::programChange(1, programs[0].load()), 0);
        midi.addEvent(juce::MidiMessage::programChange(2, programs[1].load()), 0);
        volDirty = true;
    }
    if (volDirty.exchange(false)) {
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, (int)std::round(vol[0].load() * 127)), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(2, 7, (int)std::round(vol[1].load() * 127)), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(10, 7, (int)std::round(vol[2].load() * 127)), 0);
    }

    if (playing.load()) {
        const juce::ScopedLock sl(seqLock);
        const double beatsPerSample = tempo.load() / 60.0 / sr;
        double pos = beatPos.load();
        const double endPos = pos + info.numSamples * beatsPerSample;
        // note-offs due in this block
        for (size_t i = 0; i < offs.size();) {
            if (offs[i].beat < endPos) {
                int smp = juce::jlimit(0, info.numSamples - 1, (int)((offs[i].beat - pos) / beatsPerSample));
                midi.addEvent(juce::MidiMessage::noteOff(offs[i].ch, offs[i].note), smp);
                offs[i] = offs.back(); offs.pop_back();
            } else i++;
        }
        // note-ons
        while (nextEvent < sequence.events.size() && sequence.events[nextEvent].beat < endPos) {
            const Event& e = sequence.events[nextEvent++];
            int ch = channelFor(e.part);
            int smp = juce::jlimit(0, info.numSamples - 1, (int)((e.beat - pos) / beatsPerSample));
            midi.addEvent(juce::MidiMessage::noteOff(ch, e.note), juce::jmax(0, smp - 1));   // retrigger-safe
            midi.addEvent(juce::MidiMessage::noteOn(ch, e.note, (juce::uint8)e.vel), smp);
            offs.push_back({e.beat + e.len, ch, e.note});
        }
        beatPos = endPos;
        if (endPos >= sequence.lengthBeats()) { playing = false; done = true; allNotesOff = true; }
    }
    synth->process(out, midi);
}
