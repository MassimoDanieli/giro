// Offline render of the Engine: no audio device needed. Checks timing, audio output, note-offs and stop.
#include "../app/Engine.h"
#include "../core/chart.h"
#include <cstdio>
#include <cmath>
using namespace giro;
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    int fails = 0;
    auto check = [&](bool c, const char* what) { if (!c) { fails++; std::printf("FAIL: %s\n", what); } };
    Engine engine;
    const double sr = 44100; const int block = 512;
    engine.prepareToPlay(block, sr);
    auto bars = parseChart("| F7 | Bb7 | F7 | Cm7 F7 |", 4);
    SequenceSpec spec; spec.choruses = 2; spec.countIn = true; spec.style = Style::Swing;
    auto seq = buildSequence(bars, spec);
    double len = seq.lengthBeats();
    engine.start(seq, 120.0, 32, 0);
    check(engine.isPlaying(), "playing after start");
    juce::AudioBuffer<float> buf(2, block);
    double rms = 0; int blocks = 0, firstNonSilent = -1;
    double beatAfter1s = -1;
    while (engine.isPlaying() && blocks < 20000) {
        juce::AudioSourceChannelInfo info(&buf, 0, block);
        engine.getNextAudioBlock(info);
        double e = 0; for (int i = 0; i < block; i++) e += buf.getSample(0, i) * buf.getSample(0, i);
        if (e > 1e-6 && firstNonSilent < 0) firstNonSilent = blocks;
        rms += e; blocks++;
        if (beatAfter1s < 0 && blocks * block >= sr) beatAfter1s = engine.currentBeat();
    }
    double seconds = blocks * block / sr;
    std::printf("rendered %.2fs in %d blocks, expected %.2fs; first sound at block %d; beat after 1s = %.3f (expected ~2.0)\n", seconds, blocks, len / 2.0, firstNonSilent, beatAfter1s);
    check(!engine.isPlaying() && engine.finished(), "stopped at end");
    check(std::abs(seconds - len / 2.0) < 0.1, "length matches tempo");
    check(firstNonSilent >= 0 && firstNonSilent < 4, "count-in click audible at start");
    check(std::abs(beatAfter1s - 2.0) < 0.05, "beat position tracks tempo");
    check(std::sqrt(rms / (blocks * block)) > 0.01, "audio is not silent");
    // live tempo change: 240 bpm should finish in ~half the time
    engine.start(seq, 240.0, 32, 0);
    int b2 = 0; while (engine.isPlaying() && b2 < 20000) { juce::AudioSourceChannelInfo info(&buf, 0, block); engine.getNextAudioBlock(info); b2++; }
    check(std::abs(b2 * block / sr - len / 4.0) < 0.1, "double tempo halves duration");
    // stop mid-way sends all notes off and silences within a short time
    engine.start(seq, 120.0, 32, 0);
    for (int i = 0; i < 200; i++) { juce::AudioSourceChannelInfo info(&buf, 0, block); engine.getNextAudioBlock(info); }
    engine.stop();
    double tail = 0;
    for (int i = 0; i < 100; i++) { juce::AudioSourceChannelInfo info(&buf, 0, block); engine.getNextAudioBlock(info); if (i > 60) for (int k = 0; k < block; k++) tail += std::fabs(buf.getSample(0, k)); }
    check(tail / (40 * block) < 0.01, "silent after stop");
    std::printf("backend: %s\n", engine.backendName().toRawUTF8());
    if (fails) { std::printf("%d FAILURES\n", fails); return 1; }
    std::printf("ENGINE OK\n"); return 0;
}
