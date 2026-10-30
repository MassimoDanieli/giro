#include "patterns.h"
#include "theory.h"
#include <algorithm>
#include <cmath>

namespace giro {

const char* styleName(Style s) {
    switch (s) { case Style::Swing: return "swing"; case Style::Bossa: return "bossa"; case Style::Straight: return "straight"; case Style::Ballad: return "ballad"; case Style::Shuffle: return "shuffle"; }
    return "swing";
}
const char* styleLabel(Style s) {
    switch (s) { case Style::Swing: return "Swing"; case Style::Bossa: return "Bossa"; case Style::Straight: return "Straight 8"; case Style::Ballad: return "Ballad"; case Style::Shuffle: return "Shuffle blues"; }
    return "Swing";
}
Style styleFromName(const std::string& n) {
    if (n == "bossa") return Style::Bossa;
    if (n == "straight") return Style::Straight;
    if (n == "ballad") return Style::Ballad;
    if (n == "shuffle") return Style::Shuffle;
    return Style::Swing;
}
bool styleIsSwung(Style s) { return s == Style::Swing || s == Style::Shuffle; }

void gmPrograms(Style s, int& bassProgram, int& compProgram) {
    switch (s) {
    case Style::Swing:    bassProgram = 32; compProgram = 0; break;   // acoustic bass, grand piano
    case Style::Ballad:   bassProgram = 32; compProgram = 4; break;   // acoustic bass, electric piano
    case Style::Bossa:    bassProgram = 32; compProgram = 4; break;
    case Style::Straight: bassProgram = 33; compProgram = 4; break;   // finger bass
    case Style::Shuffle:  bassProgram = 33; compProgram = 0; break;
    }
}

bool Sequence::slotAt(double beat, const BeatSlot*& slot, int& chorus) const {
    int b = (int)std::floor(beat) - countInBeats;
    if (b < 0 || b >= totalBeats || rangeBeats <= 0) return false;
    int local = b % rangeBeats;
    slot = &slots[rangeStart + local];
    chorus = b / rangeBeats + 1;
    return true;
}

namespace {

struct Builder {
    Style style; int bpb; bool swung; double AND;
    std::vector<Event>& ev;
    std::vector<int> lastVoicing;

    static int lowRoot(int pc) { return 28 + ((pc - 4 + 12) % 12); }   // E1 .. D#2
    static int bassTone(const Chord& c, const char* which) {
        int r = lowRoot(c.bassPc >= 0 ? c.bassPc : c.rootPc);
        const auto& iv = c.intervals;
        std::string w = which;
        if (w == "oct") return r + 12;
        if (w == "3") return r + (iv.size() > 1 ? iv[1] % 12 : 4);
        if (w == "5") return r + (iv.size() > 2 ? iv[2] % 12 : 7);
        if (w == "7") return r + (iv.size() > 3 ? iv[3] % 12 : 10);
        return r;
    }
    static int approach(const Chord& c, const Chord* next, int bar) {
        if (!next) return bassTone(c, "5");
        int nr = lowRoot(next->bassPc >= 0 ? next->bassPc : next->rootPc);
        int cand = bar % 2 == 1 ? nr + 1 : nr - 1;
        return cand < 26 ? nr + 1 : (cand > 45 ? nr - 1 : cand);
    }
    std::vector<int> voicing(const Chord& c) {
        std::vector<int> pcs;
        for (int iv : c.intervals) pcs.push_back((c.rootPc + iv) % 12);
        if (pcs.size() >= 4) pcs.erase(pcs.begin());      // rootless
        if (pcs.size() > 4) pcs.resize(4);
        double centre = 65;
        if (!lastVoicing.empty()) { double s = 0; for (int m : lastVoicing) s += m; centre = s / lastVoicing.size(); }
        std::vector<int> v;
        for (int pc : pcs) { int m = 60 + pc; while (m < centre - 6) m += 12; while (m > centre + 6) m -= 12; v.push_back(m); }
        std::sort(v.begin(), v.end());
        lastVoicing = v;
        return v;
    }

    void drum(double t, int note, int vel, double off) { ev.push_back({t + off, Part::Drums, note, 0.1, vel}); }
    void bass(double t, int note, double len, int vel, double off) { if (note >= 0 && note < 128) ev.push_back({t + off, Part::Bass, note, len, vel}); }
    void comp(double t, const Chord& c, double off, double len, int vel) {
        auto v = voicing(c);
        for (size_t i = 0; i < v.size(); i++) ev.push_back({t + off + i * 0.01, Part::Comp, v[i], len, vel});
    }

    void beat(double t, const BeatSlot& slot, const BeatSlot& next, int bar) {
        int b = slot.beatInBar;
        bool first = slot.chordBeat == 0, last = slot.chordBeat == slot.chordBeats - 1;
        bool changes = last && next.symbol != slot.symbol;
        Chord c = parseChord(slot.symbol), nc = parseChord(next.symbol);
        const Chord* nextC = (nc.valid && !nc.nc) ? &nc : nullptr;

        // drums
        switch (style) {
        case Style::Swing:
            drum(t, GM::Ride, (b == 0 || b == 2) ? 92 : 78, 0);
            if (b % 2 == 1 || (bpb == 3 && b == 2)) drum(t, GM::Ride, 60, AND);
            if (b % 2 == 1) drum(t, GM::Pedal, 70, 0);
            drum(t, GM::Kick, 28, 0);
            if (bar % 4 == 3 && b == bpb - 1) drum(t, GM::Snare, 55, AND);
            break;
        case Style::Shuffle:
            drum(t, GM::Hat, 88, 0); drum(t, GM::Hat, 60, AND);
            if (b % 2 == 0) drum(t, GM::Kick, 105, 0); else drum(t, GM::Snare, 100, 0);
            break;
        case Style::Straight:
            drum(t, GM::Hat, 85, 0); drum(t, GM::Hat, 58, 0.5);
            if (b == 0 || b == 2) drum(t, GM::Kick, 110, 0);
            if (b == 2 && bar % 2 == 1) drum(t, GM::Kick, 85, 0.5);
            if (b % 2 == 1) drum(t, GM::Snare, 100, 0);
            break;
        case Style::Bossa: {
            drum(t, GM::Hat, 75, 0); drum(t, GM::Hat, 55, 0.5);
            drum(t, GM::Kick, 90, 0); drum(t, GM::Kick, 65, 0.5);
            if (bar % 2 == 0) { if (b == 0) drum(t, GM::Rim, 90, 0); if (b == 1) drum(t, GM::Rim, 90, 0.5); if (b == 3) drum(t, GM::Rim, 90, 0); }
            else { if (b == 1) drum(t, GM::Rim, 90, 0); if (b == 2) drum(t, GM::Rim, 90, 0.5); }
            break; }
        case Style::Ballad:
            drum(t, GM::Hat, 55, 0);
            if (b == 0) drum(t, GM::Kick, 80, 0);
            if (b % 2 == 1) drum(t, GM::Rim, 70, 0);
            break;
        }
        if (!c.valid || c.nc) return;

        // bass
        switch (style) {
        case Style::Swing: case Style::Shuffle: {
            int n;
            if (first) n = bassTone(c, "root");
            else if (changes) n = approach(c, nextC, bar);
            else if (slot.chordBeat == 1) n = bassTone(c, slot.chordBeats > 2 ? "3" : "5");
            else if (slot.chordBeat == 2) n = bassTone(c, "5");
            else { int opts[3] = {bassTone(c, "oct"), bassTone(c, "7"), bassTone(c, "5")}; n = opts[(bar + slot.chordBeat) % 3]; }
            bass(t, n, style == Style::Shuffle ? 0.6 : 0.95, 100, 0);
            if (style == Style::Shuffle) bass(t, slot.chordBeat % 2 == 1 ? bassTone(c, "5") : n, 0.3, 75, AND);
            else if (bar % 2 == 1 && b == bpb - 1) bass(t, n == bassTone(c, "root") ? bassTone(c, "5") : std::min(n + 1, 45), 0.3, 65, AND);
            break; }
        case Style::Straight:
            if (b == 0 || b == 2) bass(t, bassTone(c, "root"), 0.9, 105, 0);
            if (b == 1) bass(t, bassTone(c, "root"), 0.4, 85, 0.5);
            if (b == 3) { if (changes) bass(t, approach(c, nextC, bar), 0.45, 95, 0.5); else bass(t, bassTone(c, "5"), 0.45, 95, 0); }
            if (bpb == 3 && b == 1) bass(t, bassTone(c, "5"), 0.9, 95, 0);
            break;
        case Style::Bossa:
            if (b % 2 == 0) bass(t, bassTone(c, "root"), 1.4, 100, 0);
            else bass(t, changes ? approach(c, nextC, bar) : bassTone(c, "5"), 0.45, 85, 0.5);
            break;
        case Style::Ballad:
            if (first) bass(t, bassTone(c, "root"), std::min(slot.chordBeats, 2) * 0.95, 95, 0);
            else if (slot.chordBeat == 2) bass(t, bassTone(c, "5"), 1.9, 85, 0);
            break;
        }

        // comping
        switch (style) {
        case Style::Swing: {
            bool played = false;
            if (bar % 2 == 0) { if (b == 0) { comp(t, c, 0, 0.5, 80); played = true; } if (b == 1) { comp(t, c, AND, 0.5, 80); played = true; } }
            else { if (b == 1) { comp(t, c, AND, 0.5, 80); played = true; } if (b == 3) { comp(t, c, 0, 0.35, 80); played = true; } }
            if (first && !played) comp(t, c, 0, 0.4, 72);
            break; }
        case Style::Shuffle: comp(t, c, 0, 0.5, 78); comp(t, c, AND, 0.3, 60); break;
        case Style::Straight:
            if (b == 0 || b == 2) comp(t, c, 0, 0.9, 82);
            if (b == 3) comp(t, c, 0.5, 0.4, 66);
            if (first && b != 0 && b != 2) comp(t, c, 0, 0.6, 76);
            break;
        case Style::Bossa: {
            bool played = false;
            if (bar % 2 == 0) { if (b == 0) { comp(t, c, 0, 0.6, 76); played = true; } if (b == 1) { comp(t, c, 0.5, 0.6, 76); played = true; } if (b == 3) { comp(t, c, 0, 0.6, 76); played = true; } }
            else { if (b == 0) { comp(t, c, 0.5, 0.6, 76); played = true; } if (b == 2) { comp(t, c, 0, 0.6, 76); played = true; } if (b == 3) { comp(t, c, 0.5, 0.6, 76); played = true; } }
            if (first && !played) comp(t, c, 0, 0.6, 72);
            break; }
        case Style::Ballad:
            if (first) comp(t, c, 0, slot.chordBeats * 0.95, 80);
            else if (slot.chordBeat == 2) comp(t, c, 0, 1.8, 58);
            break;
        }
    }
};

} // namespace

Sequence buildSequence(const std::vector<Bar>& bars, const SequenceSpec& spec) {
    Sequence seq;
    if (bars.empty()) return seq;
    int bpb = std::max(1, spec.beatsPerBar);
    seq.slots = flattenBars(bars, bpb);
    int a = spec.loopStartBar >= 0 ? std::min(spec.loopStartBar, (int)bars.size() - 1) : 0;
    int b = spec.loopEndBar >= 0 ? std::min(spec.loopEndBar, (int)bars.size() - 1) : (int)bars.size() - 1;
    if (b < a) std::swap(a, b);
    seq.rangeStart = a * bpb;
    seq.rangeBeats = (b - a + 1) * bpb;
    int choruses = spec.choruses > 0 ? spec.choruses : 100;
    seq.totalBeats = choruses * seq.rangeBeats;
    seq.countInBeats = spec.countIn ? bpb : 0;

    Builder bld{spec.style, bpb, styleIsSwung(spec.style), styleIsSwung(spec.style) ? 0.667 : 0.5, seq.events, {}};
    for (int i = 0; i < seq.countInBeats; i++) seq.events.push_back({(double)i, Part::Drums, i == 0 ? GM::ClickHi : GM::ClickLo, 0.1, i == 0 ? 110 : 85});
    double t = seq.countInBeats;
    for (int beat = 0; beat < seq.totalBeats; beat++, t += 1) {
        int local = beat % seq.rangeBeats;
        const BeatSlot& slot = seq.slots[seq.rangeStart + local];
        const BeatSlot& next = seq.slots[local + 1 < seq.rangeBeats ? seq.rangeStart + local + 1 : seq.rangeStart];
        bld.beat(t, slot, next, local / bpb);
    }
    std::stable_sort(seq.events.begin(), seq.events.end(), [](const Event& x, const Event& y) { return x.beat < y.beat; });
    return seq;
}

} // namespace giro
