#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include "../core/patterns.h"

/// A GM-ish sound source: channel 1 bass, channel 2 comping, channel 10 drums.
class SynthBackend {
public:
    virtual ~SynthBackend() = default;
    virtual void prepare(double sampleRate, int blockSize) = 0;
    virtual void process(juce::AudioBuffer<float>& out, juce::MidiBuffer& midi) = 0;
    virtual juce::String name() const = 0;
};

/// Built-in fallback synth (used where Apple's DLS synth is not available, e.g. Linux/Windows).
class SimpleSynth : public SynthBackend {
public:
    void prepare(double sampleRate, int blockSize) override;
    void process(juce::AudioBuffer<float>& out, juce::MidiBuffer& midi) override;
    juce::String name() const override { return "built-in synth"; }
private:
    struct Voice { bool active = false; int ch = 0, note = 0; double phase = 0, inc = 0; float env = 0, decay = 0, gain = 0, lp = 0, lpCoef = 0.2f; bool noise = false, held = false; int kind = 0; double pitchDrop = 0; };
    static constexpr int kVoices = 48;
    Voice voices[kVoices];
    double sr = 44100;
    float chanVol[16];
    juce::Random rng;
    void noteOn(int ch, int note, int vel);
    void noteOff(int ch, int note);
};

#if JUCE_MAC
/// Hosts Apple's DLSMusicDevice Audio Unit (the macOS General MIDI synth).
class AppleGMSynth : public SynthBackend {
public:
    static std::unique_ptr<AppleGMSynth> tryCreate(double sampleRate, int blockSize);
    void prepare(double sampleRate, int blockSize) override;
    void process(juce::AudioBuffer<float>& out, juce::MidiBuffer& midi) override;
    juce::String name() const override { return "Apple DLS General MIDI"; }
private:
    std::unique_ptr<juce::AudioPluginInstance> au;
    juce::AudioBuffer<float> scratch;
};
#endif

/// Plays a giro::Sequence with sample-accurate timing; tempo can change live.
class Engine : public juce::AudioSource {
public:
    Engine();
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

    void start(giro::Sequence seq, double bpm, int bassProgram, int compProgram);
    void stop();
    bool isPlaying() const { return playing.load(); }
    void setTempo(double bpm) { tempo.store(bpm); }
    void setVolume(giro::Part part, float v);   // 0..1
    double currentBeat() const { return beatPos.load(); }
    bool finished() const { return done.load(); }
    juce::String backendName() const;

private:
    juce::CriticalSection seqLock;
    giro::Sequence sequence;                 // swapped under seqLock while stopped
    size_t nextEvent = 0;
    struct PendingOff { double beat; int ch, note; };
    std::vector<PendingOff> offs;
    std::atomic<bool> playing{false}, done{false}, startRequested{false}, allNotesOff{false};
    std::atomic<double> tempo{120}, beatPos{0};
    std::atomic<float> vol[3]{0.9f, 0.6f, 0.7f};
    std::atomic<bool> volDirty{true};
    std::atomic<int> programs[2]{32, 0};
    std::unique_ptr<SynthBackend> synth;
    double sr = 44100;
    static int channelFor(giro::Part p) { return p == giro::Part::Bass ? 1 : (p == giro::Part::Comp ? 2 : 10); }
};
