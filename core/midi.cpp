#include "midi.h"
#include "theory.h"
#include <map>
#include <set>
#include <cmath>
#include <algorithm>

namespace giro {

namespace {
struct Reader {
    const std::vector<uint8_t>& d; size_t p = 0;
    explicit Reader(const std::vector<uint8_t>& b) : d(b) {}
    bool eof() const { return p >= d.size(); }
    uint8_t u8() { return p < d.size() ? d[p++] : (p++, 0); }
    uint16_t u16() { uint16_t v = (uint16_t)(u8() << 8); return v | u8(); }
    uint32_t u32() { uint32_t v = 0; for (int i = 0; i < 4; i++) v = (v << 8) | u8(); return v; }
    uint32_t vlq() { uint32_t v = 0; uint8_t b; int n = 0; do { b = u8(); v = (v << 7) | (b & 0x7f); } while ((b & 0x80) && ++n < 4); return v; }
    std::string str(size_t n) { std::string s; for (size_t i = 0; i < n; i++) s += (char)u8(); return s; }
};
}

bool parseMidi(const std::vector<uint8_t>& bytes, MidiFile& out, std::string& error) {
    Reader r(bytes);
    if (bytes.size() < 14 || r.str(4) != "MThd") { error = "Non è un file MIDI (manca MThd)"; return false; }
    uint32_t hlen = r.u32();
    out.format = r.u16(); int ntrk = r.u16(); uint16_t div = r.u16();
    if (div & 0x8000) { error = "Divisione SMPTE non supportata"; return false; }
    out.ppq = div ? div : 480;
    r.p += hlen > 6 ? hlen - 6 : 0;

    struct TempoEv { uint32_t tick; double bpm; };
    std::vector<TempoEv> tempos;
    bool sawTs = false, sawTitle = false;
    std::vector<std::pair<uint32_t, uint32_t>> rawNotes; // placeholder not used
    struct RawNote { uint32_t start, end; int pitch, vel, ch; };
    std::vector<RawNote> notes;

    for (int t = 0; t < ntrk && !r.eof(); t++) {
        if (r.str(4) != "MTrk") { error = "Chunk traccia non valido"; return false; }
        uint32_t len = r.u32(); size_t end = r.p + len; uint32_t tick = 0; uint8_t status = 0;
        std::map<int, RawNote> open;
        while (r.p < end && r.p < bytes.size()) {
            tick += r.vlq();
            uint8_t b = bytes[r.p];
            if (b < 0x80) b = status; else { r.p++; if (b < 0xf0) status = b; }
            if (b == 0xff) {
                uint8_t type = r.u8(); uint32_t l = r.vlq(); size_t start = r.p; r.p += l;
                if (start + l > bytes.size()) break;
                if (type == 0x51 && l >= 3) { uint32_t us = (bytes[start] << 16) | (bytes[start + 1] << 8) | bytes[start + 2]; if (us) tempos.push_back({tick, 60e6 / us}); }
                else if (type == 0x58 && l >= 2 && !sawTs) { out.num = bytes[start]; out.den = 1 << bytes[start + 1]; sawTs = true; }
                else if (type == 0x03 && !sawTitle) { std::string s(bytes.begin() + start, bytes.begin() + start + l); if (s.find_first_not_of(" \t") != std::string::npos) { out.title = s; sawTitle = true; } }
                else if (type == 0x59 && l >= 2 && out.keySig.empty()) {
                    int sf = (int8_t)bytes[start]; bool minor = bytes[start + 1] == 1;
                    static const char* MAJ[] = {"C", "G", "D", "A", "E", "B", "F#", "C#"}; static const char* FMAJ[] = {"C", "F", "Bb", "Eb", "Ab", "Db", "Gb", "Cb"};
                    static const char* MIN[] = {"A", "E", "B", "F#", "C#", "G#", "D#", "A#"}; static const char* FMIN[] = {"A", "D", "G", "C", "F", "Bb", "Eb", "Ab"};
                    int i = std::abs(sf);
                    if (i < 8) out.keySig = std::string(sf >= 0 ? (minor ? MIN[i] : MAJ[i]) : (minor ? FMIN[i] : FMAJ[i])) + (minor ? "m" : "");
                }
            } else if (b == 0xf0 || b == 0xf7) { uint32_t l = r.vlq(); r.p += l; }
            else {
                uint8_t hi = b & 0xf0, ch = b & 0x0f;
                if (hi == 0x90 || hi == 0x80) {
                    int note = r.u8(), vel = r.u8();
                    int key = ch * 128 + note;
                    if (hi == 0x90 && vel > 0) open[key] = {tick, 0, note, vel, ch};
                    else { auto it = open.find(key); if (it != open.end()) { it->second.end = tick; if (ch != 9) notes.push_back(it->second); open.erase(it); } }
                } else if (hi == 0xc0 || hi == 0xd0) r.p += 1;
                else r.p += 2;
            }
        }
        for (auto& kv : open) { if (kv.second.ch != 9) { kv.second.end = tick; notes.push_back(kv.second); } }
        r.p = end;
    }
    std::sort(tempos.begin(), tempos.end(), [](const TempoEv& a, const TempoEv& b) { return a.tick < b.tick; });
    if (!tempos.empty()) out.bpm = std::round(tempos[0].bpm);
    double ppq = out.ppq;
    for (auto& n : notes) out.notes.push_back({n.start / ppq, std::max(n.end, n.start + 1) / ppq, n.pitch, n.vel, n.ch});
    return true;
}

DetectedSong detectChords(const MidiFile& midi) {
    DetectedSong song;
    song.title = midi.title; song.tempo = midi.bpm; song.beatsPerBar = std::max(1, midi.num);
    if (midi.notes.empty()) { song.key = "C"; return song; }

    const double beatLen = 4.0 / midi.den;        // one chart beat in quarter notes
    const int bpb = song.beatsPerBar;
    double firstBeat = 1e18, lastBeat = 0;
    for (auto& n : midi.notes) { firstBeat = std::min(firstBeat, n.start / beatLen); lastBeat = std::max(lastBeat, n.end / beatLen); }
    double offset = std::floor(firstBeat / bpb) * bpb;      // in chart beats
    int nBars = std::max(1, (int)std::ceil((lastBeat - offset) / bpb));
    int nBeats = nBars * bpb;

    struct BeatInfo { double w[12] = {0}; double total = 0; int low = -1; double lowW = 0; };
    std::vector<BeatInfo> beats(nBeats);
    for (auto& n : midi.notes) {
        double s = n.start / beatLen - offset, e = std::max(n.end / beatLen - offset, s + 0.001);
        int b0 = std::max(0, (int)std::floor(s)), b1 = std::min((int)std::ceil(e), nBeats);
        for (int b = b0; b < b1; b++) {
            double ov = std::min(e, (double)b + 1) - std::max(s, (double)b);
            if (ov <= 0) continue;
            double w = ov * (0.5 + n.vel / 254.0);
            BeatInfo& bt = beats[b];
            bt.w[n.pitch % 12] += w; bt.total += w;
            if (bt.low < 0 || n.pitch < bt.low || (n.pitch == bt.low && w > bt.lowW)) { bt.low = n.pitch; bt.lowW = w; }
        }
    }

    static const char* QUALS[] = {"maj", "m", "7", "maj7", "m7", "dim", "m7b5", "dim7", "aug", "sus4", "6", "m6", "7sus4", "mMaj7"};
    struct Tpl { int r; std::string q; std::set<int> pcs; };
    std::vector<Tpl> templates;
    for (int r = 0; r < 12; r++) for (const char* q : QUALS) { Tpl t{r, q, {}}; for (int iv : *intervalsFor(q)) t.pcs.insert((iv + r) % 12); templates.push_back(t); }
    auto score = [](const BeatInfo& bt, const Tpl& t) {
        if (bt.total < 1e-6) return -1.0;
        double present = 0; int missing = 0;
        for (int pc : t.pcs) { present += bt.w[pc]; if (bt.w[pc] < 0.06 * bt.total) missing++; }
        double pf = present / bt.total;
        double s = pf * 1.6 - 0.6 - 0.32 * missing / (double)t.pcs.size();
        if (bt.low >= 0 && bt.low % 12 == t.r) s += 0.12;
        if (t.pcs.size() >= 4 && pf >= 0.95) s += 0.05;
        if (t.q == "maj" || t.q == "m") s += 0.03;
        return s;
    };

    struct Pick { int r = -1; std::string q; int bass = -1; };
    std::vector<Pick> chosen; Pick prev;
    for (auto& bt : beats) {
        if (bt.total < 1e-6) { chosen.push_back(prev); continue; }
        const Tpl* best = nullptr; double bestS = -1e9, prevS = -1e9;
        for (auto& t : templates) {
            double s = score(bt, t);
            if (s > bestS) { bestS = s; best = &t; }
            if (prev.r >= 0 && t.r == prev.r && t.q == prev.q) prevS = s;
        }
        Pick p{best->r, best->q, -1};
        if (prev.r >= 0 && prevS >= bestS - 0.15) { p.r = prev.r; p.q = prev.q; }
        if (bt.low >= 0 && bt.low % 12 != p.r) {
            int nz = 0; for (double w : bt.w) if (w > 0) nz++;
            if (bt.lowW > 0.35 * bt.total / std::max(1, nz)) p.bass = bt.low % 12;
        }
        chosen.push_back(p); prev = p;
    }

    double prof[12] = {0};
    for (auto& bt : beats) for (int i = 0; i < 12; i++) prof[i] += bt.w[i];
    static const double MAJ[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
    static const double MIN[12] = {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};
    std::string bestKey = "C"; double bestCorr = -1e18;
    for (int k = 0; k < 12; k++) {
        double cM = 0, cm = 0;
        for (int i = 0; i < 12; i++) { cM += MAJ[i] * prof[(i + k) % 12]; cm += MIN[i] * prof[(i + k) % 12]; }
        if (cM > bestCorr) { bestCorr = cM; bestKey = pcName(k, false); }
        if (cm > bestCorr) { bestCorr = cm; bestKey = pcName(k, false) + "m"; }
    }
    if (!midi.keySig.empty()) bestKey = midi.keySig;
    bool flats = keyPrefersFlats(bestKey);
    song.key = bestKey;
    auto fmt = [&](const Pick& p) {
        if (p.r < 0) return std::string("NC");
        std::string s = pcName(p.r, flats) + displayQuality(p.q);
        if (p.bass >= 0) s += "/" + pcName(p.bass, flats);
        return s;
    };

    for (int bar = 0; bar < nBars; bar++) {
        Bar b;
        for (int i = 0; i < bpb; i++) {
            std::string sym = fmt(chosen[bar * bpb + i]);
            if (!b.chords.empty() && b.chords.back().symbol == sym) b.chords.back().beats++;
            else b.chords.push_back({sym, 1});
        }
        for (size_t i = 1; b.chords.size() >= 3 && i + 1 < b.chords.size();) {
            if (b.chords[i].beats == 1 && b.chords[i - 1].symbol == b.chords[i + 1].symbol) {
                b.chords[i - 1].beats += b.chords[i].beats + b.chords[i + 1].beats;
                b.chords.erase(b.chords.begin() + i, b.chords.begin() + i + 2);
            } else i++;
        }
        song.bars.push_back(b);
    }
    return song;
}

} // namespace giro
