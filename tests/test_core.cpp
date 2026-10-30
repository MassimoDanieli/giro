#include "../core/theory.h"
#include "../core/chart.h"
#include "../core/midi.h"
#include "../core/patterns.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>

using namespace giro;
static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { fails++; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { fails++; std::printf("FAIL %s:%d  %s == %s\n", __FILE__, __LINE__, #a, #b); } } while (0)

static std::string join(const std::vector<Bar>& bars) {
    std::string s;
    for (auto& b : bars) { if (!s.empty()) s += " | "; for (size_t i = 0; i < b.chords.size(); i++) { if (i) s += " "; s += b.chords[i].symbol; } }
    return s;
}

int main() {
    // chords
    CHECK_EQ(parseChord("Cmaj7").intervals, (std::vector<int>{0, 4, 7, 11}));
    CHECK_EQ(parseChord("F#m7b5").rootPc, 6);
    CHECK_EQ(parseChord("Bb7/D").bassPc, 2);
    CHECK_EQ(parseChord("G7(b9)").quality, std::string("7b9"));
    CHECK_EQ(parseChord("E♭7").root, std::string("Eb"));
    CHECK_EQ(parseChord("CΔ7").quality, std::string("maj7"));
    CHECK(!parseChord("H7").valid);
    CHECK(parseChord("NC").nc);
    CHECK_EQ(parseChord("Dm7").transposed(2, false), std::string("Em7"));
    CHECK_EQ(parseChord("C7/E").transposed(-2, true), std::string("Bb7/D"));
    CHECK_EQ(parseChord("F7").transposed(-5, false), std::string("C7"));
    CHECK_EQ(parseChord("Cmaj7").pretty(), std::string("CΔ7"));
    CHECK_EQ(parseChord("Bbm7b5").pretty(), std::string("B♭ø7"));
    CHECK_EQ(transposeKey("F", 2), std::string("G"));
    CHECK_EQ(transposeKey("C", -2), std::string("Bb"));
    CHECK_EQ(transposeKey("Am", 3), std::string("Cm"));
    CHECK_EQ(transposeKey("F", -5), std::string("C"));

    // chart
    auto bars = parseChart("[A]\n| Dm7 G7 | Cmaj7 | % | A7 _ _ Eb7 |\n[B] F7 | Bb7 x2", 4);
    CHECK_EQ(bars.size(), (size_t)6);
    CHECK_EQ(bars[0].section, std::string("A"));
    CHECK_EQ(bars[0].chords[0].beats, 2);
    CHECK_EQ(bars[2].chords[0].symbol, std::string("Cmaj7"));
    CHECK_EQ(bars[3].chords[0].beats, 3);
    CHECK_EQ(bars[3].chords[1].beats, 1);
    CHECK_EQ(bars[4].section, std::string("B"));
    CHECK_EQ(bars[5].chords.size(), (size_t)1);
    std::string txt = chartToText(bars, 4);
    CHECK(parseChart(txt, 4) == bars);
    CHECK_EQ(flattenBars(bars, 4).size(), (size_t)24);
    auto intro = parseChart("Intro: C | G7\nA: F", 4);
    CHECK_EQ(intro.size(), (size_t)3);
    CHECK_EQ(intro[0].section, std::string("Intro"));
    CHECK_EQ(intro[2].section, std::string("A"));

    // transposition of a whole chart: Blues in F -> C
    auto blues = parseChart("| F7 | Bb7 | F7 | Cm7 F7 |\n| Bb7 | Bdim7 | F7 | Am7b5 D7 |\n| Gm7 | C7 | F7 D7 | Gm7 C7 |", 4);
    auto inC = transposeBars(blues, -5, keyPrefersFlats(transposeKey("F", -5)));
    CHECK_EQ(join(inC), std::string("C7 | F7 | C7 | Gm7 C7 | F7 | F#dim7 | C7 | Em7b5 A7 | Dm7 | G7 | C7 A7 | Dm7 G7"));
    CHECK_EQ(join(transposeBars(inC, 5, true)), join(blues));

    // sequence
    SequenceSpec spec; spec.style = Style::Swing; spec.choruses = 2; spec.countIn = true;
    auto seq = buildSequence(blues, spec);
    CHECK_EQ(seq.countInBeats, 4);
    CHECK_EQ(seq.totalBeats, 96);
    CHECK(seq.events.size() > 400);
    const BeatSlot* slot; int chorus;
    CHECK(!seq.slotAt(2.5, slot, chorus));
    CHECK(seq.slotAt(4.0, slot, chorus)); CHECK_EQ(slot->barIndex, 0); CHECK_EQ(chorus, 1);
    CHECK(seq.slotAt(4 + 48 + 14.2, slot, chorus)); CHECK_EQ(slot->barIndex, 3); CHECK_EQ(slot->chordIndex, 1); CHECK_EQ(chorus, 2);
    CHECK(!seq.slotAt(4 + 96, slot, chorus));
    for (auto& e : seq.events) { CHECK(e.note >= 0 && e.note < 128); CHECK(e.vel > 0 && e.vel < 128); CHECK(e.beat >= 0); }
    // bass stays in range, walking line changes every beat in swing
    int bassNotes = 0; for (auto& e : seq.events) if (e.part == Part::Bass) { bassNotes++; CHECK(e.note >= 26 && e.note <= 52); }
    CHECK(bassNotes >= 96);
    // loop range
    SequenceSpec lp = spec; lp.loopStartBar = 8; lp.loopEndBar = 11; lp.choruses = 1; lp.countIn = false;
    auto seq2 = buildSequence(blues, lp);
    CHECK_EQ(seq2.totalBeats, 16);
    CHECK(seq2.slotAt(0, slot, chorus)); CHECK_EQ(slot->barIndex, 8);
    for (Style st : {Style::Bossa, Style::Straight, Style::Ballad, Style::Shuffle}) { SequenceSpec s2 = spec; s2.style = st; CHECK(buildSequence(blues, s2).events.size() > 100); }
    SequenceSpec s3 = spec; s3.beatsPerBar = 3;
    CHECK(buildSequence(parseChart("| G | Em7 | Am7 | D7 |", 3), s3).events.size() > 50);

    // --- MIDI: build a file | Cmaj7 | Dm7 G7 | Em7 | A7 | Fmaj7 | G7 | C | C | at 120 bpm
    std::vector<uint8_t> body;
    auto vlq = [&](uint32_t n) { std::vector<uint8_t> b{(uint8_t)(n & 0x7f)}; while (n >>= 7) b.insert(b.begin(), (uint8_t)((n & 0x7f) | 0x80)); body.insert(body.end(), b.begin(), b.end()); };
    struct Ev { uint32_t tick; std::vector<uint8_t> bytes; };
    std::vector<Ev> evs;
    evs.push_back({0, {0xff, 0x51, 3, 0x07, 0xa1, 0x20}});
    evs.push_back({0, {0xff, 0x58, 4, 4, 2, 24, 8}});
    evs.push_back({0, {0xff, 0x03, 9, 'T', 'e', 's', 't', ' ', 'T', 'u', 'n', 'e'}});
    const int ppq = 480;
    struct Step { const char* sym; int beats; };
    std::vector<std::vector<Step>> prog = {{{"Cmaj7", 4}}, {{"Dm7", 2}, {"G7", 2}}, {{"Em7", 4}}, {{"A7", 4}}, {{"Fmaj7", 4}}, {{"G7", 4}}, {{"C", 4}}, {{"C", 4}}};
    uint32_t t = 0;
    for (auto& bar : prog) for (auto& st : bar) {
        Chord ch = parseChord(st.sym);
        std::vector<int> notes; for (int iv : ch.intervals) notes.push_back(60 + (ch.rootPc + iv) % 12); notes.push_back(36 + ch.rootPc);
        for (int n : notes) evs.push_back({t, {0x90, (uint8_t)n, 90}});
        uint32_t end = t + st.beats * ppq - 10;
        for (int n : notes) evs.push_back({end, {0x80, (uint8_t)n, 0}});
        t += st.beats * ppq;
    }
    evs.push_back({t, {0xff, 0x2f, 0}});
    std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.tick < b.tick; });
    uint32_t last = 0;
    for (auto& e : evs) { vlq(e.tick - last); body.insert(body.end(), e.bytes.begin(), e.bytes.end()); last = e.tick; }
    std::vector<uint8_t> file = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, (uint8_t)(ppq >> 8), (uint8_t)(ppq & 255), 'M', 'T', 'r', 'k',
                                 (uint8_t)(body.size() >> 24), (uint8_t)(body.size() >> 16), (uint8_t)(body.size() >> 8), (uint8_t)(body.size() & 255)};
    file.insert(file.end(), body.begin(), body.end());
    { std::ofstream f("test.mid", std::ios::binary); f.write((const char*)file.data(), (std::streamsize)file.size()); }

    MidiFile mf; std::string err;
    CHECK(parseMidi(file, mf, err));
    CHECK_EQ(mf.ppq, 480);
    CHECK_EQ(mf.bpm, 120.0);
    CHECK_EQ(mf.title, std::string("Test Tune"));
    CHECK_EQ(mf.notes.size(), (size_t)43);
    auto det = detectChords(mf);
    CHECK_EQ(det.bars.size(), (size_t)8);
    CHECK_EQ(det.key, std::string("C"));
    CHECK_EQ(det.tempo, 120.0);
    CHECK_EQ(join(det.bars), std::string("Cmaj7 | Dm7 G7 | Em7 | A7 | Fmaj7 | G7 | C | C"));
    std::printf("%s\n", chartToText(det.bars, 4).c_str());
    std::vector<uint8_t> junk = {1, 2, 3};
    CHECK(!parseMidi(junk, mf, err));

    if (fails) { std::printf("%d FAILURES\n", fails); return 1; }
    std::printf("ALL OK\n");
    return 0;
}
