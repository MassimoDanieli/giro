#include "ireal.h"
#include "theory.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <cmath>
#include <cstring>

namespace giro {

static const char* kPrefix = "1r34LbKcu7";

// ------------------------------------------------------------------ small helpers

std::string irealPercentDecode(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
            out += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16); i += 2;
        } else out += s[i];
    }
    return out;
}

static std::string htmlUnescape(std::string s) {
    static const std::pair<const char*, const char*> ents[] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&apos;", "'"}};
    for (auto& e : ents) { size_t p = 0; while ((p = s.find(e.first, p)) != std::string::npos) { s.replace(p, strlen(e.first), e.second); p += strlen(e.second); } }
    return s;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static std::vector<std::string> splitStr(const std::string& s, const std::string& sep) {
    std::vector<std::string> out; size_t p = 0, q;
    while ((q = s.find(sep, p)) != std::string::npos) { out.push_back(s.substr(p, q - p)); p = q + sep.size(); }
    out.push_back(s.substr(p));
    return out;
}

static std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) { s.replace(p, from.size(), to); p += to.size(); }
    return s;
}

static std::string lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

// ------------------------------------------------------------------ obfuscation

static std::string obfusc50(const std::string& b) {
    std::string r = b;
    for (int i = 0; i < 5; i++) { r[(size_t)i] = b[(size_t)(49 - i)]; r[(size_t)(49 - i)] = b[(size_t)i]; }
    for (int i = 10; i < 24; i++) { r[(size_t)i] = b[(size_t)(49 - i)]; r[(size_t)(49 - i)] = b[(size_t)i]; }
    return r;
}

std::string irealUnscramble(const std::string& in) {
    std::string s = in, out;
    while (s.size() > 50) {
        std::string chunk = s.substr(0, 50); s = s.substr(50);
        out += s.size() < 2 ? chunk : obfusc50(chunk);
    }
    return out + s;
}

// ------------------------------------------------------------------ style / tempo / chords

const char* irealStyleToGiro(const std::string& styleIn) {
    std::string s = lower(styleIn);
    auto has = [&](const char* w) { return s.find(w) != std::string::npos; };
    if (has("bossa") || has("latin") || has("samba") || has("afro") || has("calypso") || has("reggae")) return "bossa";
    if (has("ballad") || (has("slow") && !has("blues"))) return "ballad";
    if (has("shuffle") || has("blues")) return "shuffle";
    if (has("rock") || has("funk") || has("pop") || has("even") || has("gospel") || has("r&b") || has("soul") || has("disco") || has("country")) return "straight";
    return "swing";
}

double irealDefaultTempo(const std::string& styleIn) {
    std::string s = lower(styleIn);
    auto has = [&](const char* w) { return s.find(w) != std::string::npos; };
    if (has("up tempo") || has("uptempo") || has("fast")) return 220;
    if (has("medium up")) return 180;
    if (has("ballad")) return 70;
    if (has("slow")) return 90;
    if (has("waltz")) return 150;
    if (has("bossa")) return 130;
    if (has("samba") || has("latin") || has("afro")) return 160;
    if (has("rock") || has("funk") || has("pop") || has("even")) return 110;
    if (has("shuffle") || has("blues")) return 120;
    return 140;
}

std::string irealChordToGiro(const std::string& in) {
    std::string s = trim(in);
    if (s.empty()) return "";
    if (s == "n" || s == "N.C." || s == "NC") return "NC";
    // strip alternate chord in parentheses
    size_t par = s.find('(');
    if (par != std::string::npos) { size_t close = s.find(')', par); s = s.substr(0, par) + (close == std::string::npos ? "" : s.substr(close + 1)); s = trim(s); }
    if (s.empty()) return "";
    // slash bass
    std::string bass;
    size_t slash = s.rfind('/');
    if (slash != std::string::npos) { bass = s.substr(slash + 1); s = s.substr(0, slash); }
    if (s.empty() || s[0] == 'W') {   // W = invisible root: show the bass note alone
        return bass.empty() ? "NC" : bass;
    }
    if (s[0] < 'A' || s[0] > 'G') return "";
    std::string root = s.substr(0, 1); size_t p = 1;
    if (p < s.size() && (s[p] == 'b' || s[p] == '#')) { root += s[p]; p++; }
    std::string q = s.substr(p);
    // iReal spellings our parser does not know by default
    static const std::map<std::string, std::string> extra = {
        {"2", "add9"}, {"^13", "maj13"}, {"-^7", "mMaj7"}, {"-^9", "mMaj7"}, {"^9#11", "maj7#11"}, {"9#11", "7#11"}, {"13#11", "7#11"},
        {"7b9sus", "7sus4"}, {"7b13sus", "7sus4"}, {"13sus", "9sus4"}, {"7susadd3", "7sus4"}, {"-b6", "m"}, {"-69", "m69"}, {"-11", "m11"},
        {"h9", "m7b5"}, {"o^7", "dim7"}, {"7b9b13", "7b9"}, {"7#9b13", "7#9"}, {"7b9#9", "7b9"}, {"7b9#11", "7b9"}, {"9b5", "7b5"}, {"9#5", "7#5"},
        {"^7#5", "maj7#5"}, {"6", "6"}, {"7b9#5", "7b9#5"}, {"7#9#5", "7#9#5"}, {"+", "aug"}, {"7+", "7#5"}
    };
    std::string quality;
    auto it = extra.find(q);
    if (it != extra.end()) quality = displayQuality(it->second);
    else {
        Chord c = parseChord(root + q);
        // parseChord falls back to maj / 7 / m7 for text it does not know: keep the typed text in that case
        bool known = q.empty() || c.quality != (q.find('7') != std::string::npos ? "7" : (q[0] == '-' ? "m7" : "maj")) || q == "7" || q == "-7" || q == "maj" || q == "M";
        quality = known ? displayQuality(c.quality) : q;
        if (q == "-") quality = "m";
    }
    std::string out = root + quality;
    if (!bass.empty()) out += "/" + bass;
    return out;
}

// ------------------------------------------------------------------ chart parsing

namespace {

struct Cell { std::string chord; };           // chord text; "" = empty cell (hold)
struct Measure {
    std::vector<Cell> cells;
    std::string section;                      // "A", "B", "Intro" ...
    int ending = 0;                           // N1, N2, N3
    bool repeatOpen = false, repeatClose = false, sectionOpen = false;
    bool segno = false, coda = false;
    int ts = 0;                               // beats per bar announced in this measure (0 = unchanged)
    bool repeatPrev1 = false, repeatPrev2 = false;   // x / r
    bool empty() const { for (auto& c : cells) if (!c.chord.empty()) return false; return cells.empty() || true; }
    bool hasChord() const { for (auto& c : cells) if (!c.chord.empty()) return true; return false; }
};

int tsBeats(const std::string& t) {   // "44" -> 4, "34" -> 3, "68" -> 2, "12" -> 4 (12/8)
    if (t == "12") return 4;
    if (t.size() != 2) return 4;
    int num = t[0] - '0', den = t[1] - '0';
    if (den == 8) return std::max(1, num / 3);
    return std::max(1, num);
}

/// Reads one chord starting at raw[i] (a root letter or W); advances i past it.
std::string readChord(const std::string& raw, size_t& i) {
    std::string c(1, raw[i]); i++;
    auto at = [&](size_t k) { return k < raw.size() ? raw[k] : '\0'; };
    if (at(i) == 'b' || at(i) == '#') { c += raw[i]; i++; }
    int paren = 0;
    while (i < raw.size()) {
        char ch = raw[i];
        bool take = false;
        if (paren > 0) { take = ch != '>' && ch != '|' && ch != ']' && ch != '}' && ch != 'Z'; if (ch == ')') paren--; }
        else if (ch == '(') { paren++; take = true; }
        else if (isdigit((unsigned char)ch) || ch == '^' || ch == '-' || ch == '+' || ch == 'o' || ch == 'h' || ch == 'b' || ch == '#') take = true;
        else if (ch == '/' && at(i + 1) >= 'A' && at(i + 1) <= 'G') { c += '/'; c += at(i + 1); i += 2; if (at(i) == 'b' || at(i) == '#') { c += raw[i]; i++; } continue; }
        else if (ch == 's' && at(i + 1) == 'u' && at(i + 2) == 's') { c += "sus"; i += 3; continue; }
        else if (ch == 'a' && at(i + 1) == 'l' && at(i + 2) == 't') { c += "alt"; i += 3; continue; }
        else if (ch == 'a' && at(i + 1) == 'd' && at(i + 2) == 'd') { c += "add"; i += 3; continue; }
        else if (ch == '*' && !c.empty() && c.back() == '+') { i++; continue; }   // "+*" quirk
        if (!take) break;
        c += ch; i++;
    }
    return c;
}

std::vector<Measure> splitMeasures(const std::string& rawIn) {
    std::string raw = replaceAll(rawIn, "XyQ", "   ");
    raw = replaceAll(raw, "LZ", " |");
    raw = replaceAll(raw, "Kcl", "| x");
    raw = replaceAll(raw, "cl", "x");   // leftover one-bar repeat glyph
    std::vector<Measure> out;
    Measure cur;
    std::string pendingSection; int pendingEnding = 0; bool pendingOpen = false, pendingSectionOpen = false, pendingSegno = false, pendingCoda = false; int pendingTs = 0;
    auto flush = [&](bool close) {
        bool meaningful = cur.hasChord() || cur.repeatPrev1 || cur.repeatPrev2;
        if (meaningful) {
            cur.section = pendingSection; cur.ending = pendingEnding; cur.repeatOpen = pendingOpen; cur.sectionOpen = pendingSectionOpen;
            cur.segno = pendingSegno; cur.coda = pendingCoda; cur.ts = pendingTs; cur.repeatClose = close;
            out.push_back(cur);
            pendingSection.clear(); pendingEnding = 0; pendingOpen = false; pendingSectionOpen = false; pendingSegno = false; pendingCoda = false; pendingTs = 0;
        } else if (close && !out.empty()) out.back().repeatClose = true;
        cur = Measure();
    };
    for (size_t i = 0; i < raw.size(); i++) {
        char ch = raw[i];
        if ((ch >= 'A' && ch <= 'G') || ch == 'W') {
            size_t j = i;
            std::string chord = readChord(raw, j);
            cur.cells.push_back({chord});
            i = j - 1;
            continue;
        }
        switch (ch) {
        case '|': flush(false); break;
        case '[': flush(false); pendingSectionOpen = true; break;
        case ']': flush(false); break;
        case '{': flush(false); pendingOpen = true; break;
        case '}': flush(true); break;
        case 'Z': flush(false); break;
        case ' ': cur.cells.push_back({""}); break;
        case ',': break;
        case 'p': cur.cells.push_back({""}); break;          // slash: hold previous chord
        case 'n': cur.cells.push_back({"n"}); break;
        case 'x': cur.repeatPrev1 = true; break;
        case 'r': cur.repeatPrev2 = true; break;
        case 'S': pendingSegno = true; break;
        case 'Q': pendingCoda = true; break;
        case 'T': if (i + 2 < raw.size()) { pendingTs = tsBeats(raw.substr(i + 1, 2)); i += 2; } break;
        case '*': if (i + 1 < raw.size()) { char sc = raw[i + 1]; pendingSection = sc == 'i' ? "Intro" : std::string(1, sc); i++; } break;
        case 'N': if (i + 1 < raw.size() && isdigit((unsigned char)raw[i + 1])) { pendingEnding = raw[i + 1] - '0'; i++; } break;
        case '<': { size_t close = raw.find('>', i); i = close == std::string::npos ? raw.size() : close; break; }
        default: break;   // Y U s l f K and anything unknown
        }
    }
    flush(false);
    return out;
}

Bar measureToBar(const Measure& m, int bpb, const std::string& prevChord, std::string& lastChord) {
    Bar bar;
    std::string cur = prevChord;
    std::vector<std::pair<std::string, int>> runs;   // chord, cells
    for (auto& c : m.cells) {
        if (!c.chord.empty()) { std::string sym = irealChordToGiro(c.chord); if (sym.empty()) sym = cur.empty() ? "NC" : cur; runs.push_back({sym, 1}); cur = sym; }
        else if (!runs.empty()) runs.back().second++;
        else runs.push_back({cur.empty() ? "NC" : cur, 1});
    }
    if (runs.empty()) runs.push_back({cur.empty() ? "NC" : cur, bpb});
    int total = 0; bool anyEmpty = false;
    for (auto& r : runs) { total += r.second; if (r.second > 1) anyEmpty = true; }
    if (total < bpb && !anyEmpty && runs.size() > 1) {
        // chords written in adjacent cells with the trailing cells trimmed: the intent is an even split
        auto dist = distributeBeats((int)runs.size(), bpb);
        for (size_t i = 0; i < runs.size(); i++) runs[i].second = dist[i];
        total = bpb;
    }
    if (total < bpb) runs.back().second += bpb - total;
    else if (total > bpb) {
        // scale proportionally, keeping every chord at least one beat
        std::vector<int> beats; int acc = 0;
        for (size_t i = 0; i < runs.size(); i++) { int b = std::max(1, (int)std::lround((double)runs[i].second * bpb / total)); beats.push_back(b); acc += b; }
        while (acc > bpb) { size_t k = (size_t)(std::max_element(beats.begin(), beats.end()) - beats.begin()); if (beats[k] <= 1) break; beats[k]--; acc--; }
        while (acc < bpb) { beats.back()++; acc++; }
        for (size_t i = 0; i < runs.size(); i++) runs[i].second = beats[i];
    }
    for (auto& r : runs) {
        if (!bar.chords.empty() && bar.chords.back().symbol == r.first) bar.chords.back().beats += r.second;
        else bar.chords.push_back({r.first, r.second});
    }
    lastChord = cur;
    return bar;
}

} // namespace

static void expandSong(IRealSong& song, const std::string& raw) {
    auto measures = splitMeasures(raw);
    // time signature: first announced one
    int bpb = 4;
    for (auto& m : measures) if (m.ts) { bpb = m.ts; break; }
    song.beatsPerBar = bpb;

    // resolve x / r before anything else (they refer to the written measures)
    for (size_t i = 0; i < measures.size(); i++) {
        if (measures[i].repeatPrev1 && i >= 1) { measures[i].cells = measures[i - 1].cells; measures[i].repeatPrev1 = false; }
        if (measures[i].repeatPrev2 && i >= 2) {
            Measure second = measures[i]; second.cells = measures[i - 1].cells; second.repeatPrev2 = false; second.section.clear(); second.ending = 0; second.repeatOpen = false; second.sectionOpen = false;
            measures[i].cells = measures[i - 2].cells; measures[i].repeatPrev2 = false;
            bool closes = measures[i].repeatClose; measures[i].repeatClose = false; second.repeatClose = closes;
            // the measure after an 'r' is usually written empty and already dropped; insert the second copy
            measures.insert(measures.begin() + (long)i + 1, second);
            i++;
        }
    }

    // expand repeats with endings (two passes) and D.S./D.C. al coda
    std::vector<int> order;                 // indices into measures, in playing order
    int repeatStart = 0, sectionStart = 0;
    int firstEndingAt = -1;
    bool justRepeated = false;
    for (int i = 0; i < (int)measures.size(); i++) {
        const Measure& m = measures[i];
        if (m.repeatOpen) { repeatStart = i; firstEndingAt = -1; justRepeated = false; }
        if (m.sectionOpen || !m.section.empty()) { sectionStart = i; if (!justRepeated) { /* implicit repeat start for '}' without '{' */ } }
        if (m.ending == 1 && firstEndingAt < 0) firstEndingAt = i;
        order.push_back(i);
        if (m.repeatClose && !justRepeated) {
            int start = repeatStart;
            if (firstEndingAt >= 0 && firstEndingAt < start) start = sectionStart;
            int stop = firstEndingAt >= 0 ? firstEndingAt : i + 1;
            for (int k = start; k < stop; k++) order.push_back(k);
            justRepeated = true;
            repeatStart = i + 1; firstEndingAt = -1;
        } else if (!m.repeatClose && justRepeated && !m.repeatOpen && m.ending == 0) {
            // once we are past the second ending, a later '}' opens a new repeat from here
        }
        if (m.repeatClose) { /* next repeat starts after the endings */ }
        if (m.ending >= 2) justRepeated = false, repeatStart = i + 1;
    }
    // codas: two Q marks mean "from the segno (or top) to the first Q, then jump to the second Q"
    std::vector<int> qs; int segno = -1;
    for (int k = 0; k < (int)order.size(); k++) { const Measure& m = measures[(size_t)order[(size_t)k]]; if (m.coda) qs.push_back(k); if (m.segno && segno < 0) segno = k; }
    if (qs.size() >= 2) {
        std::vector<int> o2(order.begin(), order.begin() + qs[1]);
        for (int k = std::max(0, segno); k < qs[0]; k++) o2.push_back(order[(size_t)k]);
        for (int k = qs[1]; k < (int)order.size(); k++) o2.push_back(order[(size_t)k]);
        order = o2;
    }

    std::string last;
    for (size_t n = 0; n < order.size(); n++) {
        const Measure& m = measures[(size_t)order[n]];
        Bar b = measureToBar(m, bpb, last, last);
        if (!m.section.empty()) b.section = m.section;
        if (m.ending >= 2 && b.section.empty()) b.section = std::to_string(m.ending) + ".";
        else if (m.ending == 1 && b.section.empty()) b.section = "1.";
        song.bars.push_back(b);
    }
}

static bool parseOneSong(const std::string& body, bool newFormat, IRealSong& song) {
    auto f = splitStr(body, "=");
    if (f.size() < 5) return false;
    int c = -1;
    if (newFormat) { for (int i = 0; i < (int)f.size(); i++) if (f[(size_t)i].find(kPrefix) != std::string::npos) { c = i; break; } }
    else c = 5;
    if (c < 0 || c >= (int)f.size()) return false;
    song.title = trim(f[0]);
    song.composer = trim(f[1]);
    // "Last First" -> "First Last" when it looks like two words
    { auto w = splitStr(song.composer, " "); if (w.size() == 2) song.composer = w[1] + " " + w[0]; }
    song.style = c >= 3 ? trim(f[(size_t)c - 3]) : "";
    std::string key = c >= 2 ? trim(f[(size_t)c - 2]) : "C";
    if (!newFormat) { song.style = trim(f[2]); key = trim(f[3]); }
    if (!key.empty() && key.back() == '-') key = key.substr(0, key.size() - 1) + "m";
    if (key.empty() || noteToPc(key.back() == 'm' ? key.substr(0, key.size() - 1) : key) < 0) key = "C";
    song.key = key;
    std::string raw;
    if (newFormat) { std::string s = f[(size_t)c]; raw = irealUnscramble(s.substr(s.find(kPrefix) + strlen(kPrefix))); }
    else raw = f[(size_t)c];
    if (newFormat && c + 2 < (int)f.size()) { try { song.tempo = std::stod(trim(f[(size_t)c + 2])); } catch (...) { song.tempo = 0; } }
    song.raw = raw;
    expandSong(song, raw);
    return !song.bars.empty();
}

IRealImport parseIReal(const std::string& textIn) {
    IRealImport out;
    std::string text = htmlUnescape(textIn);
    // collect every link in the text
    std::vector<std::pair<size_t, bool>> starts;
    for (size_t p = 0; (p = text.find("ireal", p)) != std::string::npos; p++) {
        if (text.compare(p, 9, "irealb://") == 0) starts.push_back({p + 9, true});
        else if (text.compare(p, 12, "irealbook://") == 0) starts.push_back({p + 12, false});
    }
    if (starts.empty()) { out.error = "Nessun link iReal Pro trovato (irealb:// o irealbook://)."; return out; }
    for (auto& st : starts) {
        size_t prefixLen = st.second ? 9 : 12;
        char before = st.first > prefixLen ? text[st.first - prefixLen - 1] : '\0';
        size_t end = (before == '"' || before == '\'') ? text.find(before, st.first) : text.find_first_of("\"' \r\n\t", st.first);
        std::string link = text.substr(st.first, end == std::string::npos ? std::string::npos : end - st.first);
        std::string decoded = irealPercentDecode(link);
        auto parts = splitStr(decoded, "===");
        for (size_t i = 0; i < parts.size(); i++) {
            std::string part = parts[i];
            if (trim(part).empty()) continue;
            if (std::count(part.begin(), part.end(), '=') < 4) { if (i == parts.size() - 1) out.playlist = trim(part); continue; }
            IRealSong s;
            if (parseOneSong(part, st.second, s)) out.songs.push_back(s);
        }
    }
    if (out.songs.empty()) out.error = "Link iReal trovato ma nessun brano leggibile.";
    return out;
}

} // namespace giro
