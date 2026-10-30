#include "theory.h"
#include <map>
#include <set>
#include <algorithm>

namespace giro {

static const char* SHARP[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char* FLAT[12]  = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
static const std::set<std::string> FLAT_KEYS = {"F", "Bb", "Eb", "Ab", "Db", "Gb", "Cb", "Dm", "Gm", "Cm", "Fm", "Bbm", "Ebm", "Abm"};

static int mod12(int x) { return ((x % 12) + 12) % 12; }

std::string pcName(int pc, bool flats) { return (flats ? FLAT : SHARP)[mod12(pc)]; }

static bool startsWith(const std::string& s, const char* p) { return s.compare(0, std::char_traits<char>::length(p), p) == 0; }

// Reads one accidental at the start of s ("#", "b", "♭", "♯"); returns normalised "#"/"b" and consumed bytes, or 0.
static int readAccidental(const std::string& s, std::string& out) {
    if (startsWith(s, "#")) { out = "#"; return 1; }
    if (startsWith(s, "b")) { out = "b"; return 1; }
    if (startsWith(s, "♯")) { out = "#"; return 3; }   // ♯
    if (startsWith(s, "♭")) { out = "b"; return 3; }   // ♭
    return 0;
}

int noteToPc(const std::string& name) {
    static const std::map<char, int> ROOT = {{'C', 0}, {'D', 2}, {'E', 4}, {'F', 5}, {'G', 7}, {'A', 9}, {'B', 11}};
    if (name.empty()) return -1;
    auto it = ROOT.find(name[0]);
    if (it == ROOT.end()) return -1;
    int pc = it->second;
    std::string acc; int n = readAccidental(name.substr(1), acc);
    if (n == 0 && name.size() != 1) return -1;
    if (n > 0 && name.size() != 1 + (size_t)n) return -1;
    if (acc == "#") pc++; else if (acc == "b") pc--;
    return mod12(pc);
}

bool keyPrefersFlats(const std::string& key) { return FLAT_KEYS.count(key) > 0 || key.find('b') != std::string::npos; }

std::string transposeKey(const std::string& key, int semis) {
    bool minor = !key.empty() && key.back() == 'm';
    std::string base = minor ? key.substr(0, key.size() - 1) : key;
    int pc = noteToPc(base);
    if (pc < 0) return key;
    int np = pc + semis;
    std::string sharp = pcName(np, false), flat = pcName(np, true), suf = minor ? "m" : "";
    static const std::set<std::string> PLAIN_FLATS = {"F", "Bb", "Eb", "Ab", "Db"};
    bool useFlat = FLAT_KEYS.count(flat + suf) || (!FLAT_KEYS.count(sharp + suf) && flat.size() == 2 && PLAIN_FLATS.count(flat));
    return (useFlat ? flat : sharp) + suf;
}

static const std::map<std::string, std::string>& aliases() {
    static const std::map<std::string, std::string> A = {
        {"", "maj"}, {"maj", "maj"}, {"M", "maj"}, {"ma", "maj"},
        {"m", "m"}, {"-", "m"}, {"min", "m"}, {"mi", "m"},
        {"7", "7"}, {"dom7", "7"},
        {"maj7", "maj7"}, {"M7", "maj7"}, {"Δ", "maj7"}, {"Δ7", "maj7"}, {"^", "maj7"}, {"^7", "maj7"}, {"ma7", "maj7"},
        {"m7", "m7"}, {"-7", "m7"}, {"min7", "m7"}, {"mi7", "m7"},
        {"dim", "dim"}, {"°", "dim"}, {"o", "dim"},
        {"dim7", "dim7"}, {"°7", "dim7"}, {"o7", "dim7"},
        {"m7b5", "m7b5"}, {"ø", "m7b5"}, {"ø7", "m7b5"}, {"-7b5", "m7b5"}, {"h7", "m7b5"}, {"h", "m7b5"}, {"min7b5", "m7b5"},
        {"aug", "aug"}, {"+", "aug"}, {"#5", "aug"},
        {"+7", "7#5"}, {"7#5", "7#5"}, {"aug7", "7#5"}, {"7+", "7#5"},
        {"sus4", "sus4"}, {"sus", "sus4"}, {"sus2", "sus2"},
        {"7sus4", "7sus4"}, {"7sus", "7sus4"}, {"9sus4", "9sus4"}, {"9sus", "9sus4"},
        {"6", "6"}, {"maj6", "6"}, {"M6", "6"}, {"m6", "m6"}, {"-6", "m6"}, {"69", "69"}, {"6/9", "69"}, {"m69", "m69"}, {"m6/9", "m69"},
        {"9", "9"}, {"maj9", "maj9"}, {"M9", "maj9"}, {"Δ9", "maj9"}, {"^9", "maj9"}, {"m9", "m9"}, {"-9", "m9"},
        {"11", "11"}, {"m11", "m11"}, {"-11", "m11"}, {"13", "13"}, {"maj13", "maj13"}, {"^13", "maj13"}, {"m13", "m13"},
        {"7b9", "7b9"}, {"7#9", "7#9"}, {"7b5", "7b5"}, {"7#11", "7#11"}, {"7b13", "7b13"}, {"7alt", "7alt"}, {"alt", "7alt"},
        {"7b9#5", "7b9#5"}, {"7#9#5", "7#9#5"}, {"7b9b5", "7b9b5"}, {"7#9b5", "7#9b5"}, {"13b9", "13b9"}, {"7#9#11", "7#9#11"},
        {"add9", "add9"}, {"add2", "add9"}, {"madd9", "madd9"}, {"maj7#11", "maj7#11"}, {"Δ#11", "maj7#11"}, {"^#11", "maj7#11"}, {"maj7#5", "maj7#5"},
        {"mMaj7", "mMaj7"}, {"mM7", "mMaj7"}, {"-Δ7", "mMaj7"}, {"m(maj7)", "mMaj7"}, {"minMaj7", "mMaj7"}, {"-^7", "mMaj7"}, {"mΔ7", "mMaj7"}, {"-Δ", "mMaj7"}, {"-^", "mMaj7"},
        {"5", "5"}, {"power", "5"}
    };
    return A;
}

static const std::map<std::string, std::vector<int>>& intervalTable() {
    static const std::map<std::string, std::vector<int>> T = {
        {"maj", {0, 4, 7}}, {"m", {0, 3, 7}}, {"7", {0, 4, 7, 10}}, {"maj7", {0, 4, 7, 11}}, {"m7", {0, 3, 7, 10}},
        {"dim", {0, 3, 6}}, {"dim7", {0, 3, 6, 9}}, {"m7b5", {0, 3, 6, 10}}, {"aug", {0, 4, 8}}, {"7#5", {0, 4, 8, 10}},
        {"sus4", {0, 5, 7}}, {"sus2", {0, 2, 7}}, {"7sus4", {0, 5, 7, 10}}, {"9sus4", {0, 5, 7, 10, 14}},
        {"6", {0, 4, 7, 9}}, {"m6", {0, 3, 7, 9}}, {"69", {0, 4, 7, 9, 14}}, {"m69", {0, 3, 7, 9, 14}},
        {"9", {0, 4, 7, 10, 14}}, {"maj9", {0, 4, 7, 11, 14}}, {"m9", {0, 3, 7, 10, 14}},
        {"11", {0, 4, 7, 10, 14, 17}}, {"m11", {0, 3, 7, 10, 14, 17}}, {"13", {0, 4, 7, 10, 14, 21}}, {"maj13", {0, 4, 7, 11, 14, 21}}, {"m13", {0, 3, 7, 10, 14, 21}},
        {"7b9", {0, 4, 7, 10, 13}}, {"7#9", {0, 4, 7, 10, 15}}, {"7b5", {0, 4, 6, 10}}, {"7#11", {0, 4, 7, 10, 18}}, {"7b13", {0, 4, 7, 10, 20}}, {"7alt", {0, 4, 8, 10, 13, 15}},
        {"7b9#5", {0, 4, 8, 10, 13}}, {"7#9#5", {0, 4, 8, 10, 15}}, {"7b9b5", {0, 4, 6, 10, 13}}, {"7#9b5", {0, 4, 6, 10, 15}}, {"13b9", {0, 4, 7, 10, 13, 21}}, {"7#9#11", {0, 4, 7, 10, 15, 18}},
        {"add9", {0, 4, 7, 14}}, {"madd9", {0, 3, 7, 14}}, {"maj7#11", {0, 4, 7, 11, 18}}, {"maj7#5", {0, 4, 8, 11}},
        {"mMaj7", {0, 3, 7, 11}}, {"5", {0, 7}}
    };
    return T;
}

const std::vector<int>* intervalsFor(const std::string& q) {
    auto& T = intervalTable(); auto it = T.find(q); return it == T.end() ? nullptr : &it->second;
}

std::string displayQuality(const std::string& q) {
    static const std::map<std::string, std::string> D = {{"maj", ""}, {"m", "m"}, {"maj7", "maj7"}, {"m7", "m7"}, {"dim", "dim"}, {"dim7", "dim7"}, {"m7b5", "m7b5"}, {"7#5", "7#5"}, {"mMaj7", "mMaj7"}};
    auto it = D.find(q); return it == D.end() ? q : it->second;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
static std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) { s.replace(p, from.size(), to); p += to.size(); }
    return s;
}

Chord parseChord(const std::string& raw) {
    Chord c; c.symbol = trim(raw);
    const std::string& s = c.symbol;
    if (s.empty() || s == "NC" || s == "N.C." || s == "nc") { c.nc = true; if (s.empty()) c.symbol = "NC"; return c; }
    if (noteToPc(std::string(1, s[0])) < 0) { c.valid = false; c.qualityText = s; return c; }
    c.root = s.substr(0, 1);
    size_t p = 1;
    std::string acc; int n = readAccidental(s.substr(1), acc);
    if (n) { c.root += acc; p += n; }
    std::string rest = s.substr(p);
    size_t slash = rest.rfind('/');
    if (slash != std::string::npos) {
        std::string after = rest.substr(slash + 1);
        if (noteToPc(after) >= 0) {
            c.bass = replaceAll(replaceAll(after, "♭", "b"), "♯", "#");
            c.bassPc = noteToPc(after);
            rest = rest.substr(0, slash);
        }
    }
    c.qualityText = trim(rest);
    auto& A = aliases();
    auto it = A.find(c.qualityText);
    if (it == A.end()) it = A.find(replaceAll(replaceAll(c.qualityText, "(", ""), ")", ""));
    if (it != A.end()) c.quality = it->second;
    else {
        const std::string& q = c.qualityText;
        bool minorish = (startsWith(q, "m") && !startsWith(q, "maj")) || startsWith(q, "-") || startsWith(q, "min");
        c.quality = minorish ? "m7" : (q.find('7') != std::string::npos ? "7" : "maj");
    }
    c.rootPc = noteToPc(c.root);
    if (auto iv = intervalsFor(c.quality)) c.intervals = *iv; else c.intervals = {0, 4, 7};
    return c;
}

std::string Chord::transposed(int semis, bool flats) const {
    if (nc || !valid) return symbol;
    std::string out = pcName(rootPc + semis, flats) + qualityText;
    if (bassPc >= 0) out += "/" + pcName(bassPc + semis, flats);
    return out;
}

std::string Chord::pretty() const {
    if (nc) return "N.C.";
    if (!valid) return symbol;
    std::string q = qualityText;
    q = replaceAll(q, "maj7", "Δ7"); q = replaceAll(q, "m7b5", "ø7"); q = replaceAll(q, "dim", "°");
    q = replaceAll(q, "b", "♭"); q = replaceAll(q, "#", "♯");
    std::string out = replaceAll(replaceAll(root, "b", "♭"), "#", "♯") + q;
    if (!bass.empty()) out += "/" + replaceAll(replaceAll(bass, "b", "♭"), "#", "♯");
    return out;
}

} // namespace giro
