#include "chart.h"
#include "theory.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace giro {

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out; std::string cur;
    for (char ch : s) { if (ch == sep) { out.push_back(cur); cur.clear(); } else cur += ch; }
    out.push_back(cur); return out;
}
static bool isRepeatMarker(const std::string& t) {   // x2, X4 ...
    if (t.size() < 2 || (t[0] != 'x' && t[0] != 'X')) return false;
    for (size_t i = 1; i < t.size(); i++) if (!isdigit((unsigned char)t[i])) return false;
    return true;
}

std::vector<int> distributeBeats(int n, int beatsPerBar) {
    if (n <= 0) return {};
    if (n == 1) return {beatsPerBar};
    if (beatsPerBar == 4 && n == 3) return {2, 1, 1};
    int base = beatsPerBar / n;
    std::vector<int> out(n, std::max(base, 0));
    int rem = beatsPerBar - base * n;
    for (int i = 0; i < n && rem > 0; i++, rem--) out[i]++;
    return out;
}

std::vector<Bar> parseChart(const std::string& text, int beatsPerBar) {
    std::vector<Bar> bars;
    std::string section; bool haveSection = false;
    int prevIdx = -1;
    std::istringstream in(text);
    std::string rawLine;
    while (std::getline(in, rawLine)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        if (line[0] == '[') {
            size_t close = line.find(']');
            if (close != std::string::npos) {
                section = trim(line.substr(1, close - 1)); haveSection = true;
                line = trim(line.substr(close + 1));
                if (line.empty()) continue;
            }
        } else if (isalpha((unsigned char)line[0])) {
            // "A:" / "Intro:" style section marker (letters, digits, spaces, apostrophes, then a colon, within 21 chars)
            size_t i = 0;
            while (i < line.size() && i < 21 && (isalnum((unsigned char)line[i]) || line[i] == ' ' || line[i] == '\'' || line[i] == '_')) i++;
            if (i < line.size() && line[i] == ':') {
                section = trim(line.substr(0, i)); haveSection = true;
                line = trim(line.substr(i + 1));
                if (line.empty()) continue;
            }
        }
        for (const std::string& rawCell : split(line, '|')) {
            std::string cell = trim(rawCell);
            if (cell.empty() || isRepeatMarker(cell)) continue;
            Bar bar;
            if (cell == "%" || cell == "/") {
                if (prevIdx >= 0) bar.chords = bars[prevIdx].chords; else bar.chords = {{"NC", beatsPerBar}};
            } else {
                std::vector<std::string> toks;
                for (const std::string& t : split(cell, ' ')) if (!t.empty() && !isRepeatMarker(t)) toks.push_back(t);
                if (toks.empty()) continue;
                bool held = std::find(toks.begin(), toks.end(), "_") != toks.end();
                if (held) {
                    for (const std::string& t : toks) {
                        if (t == "_") { if (!bar.chords.empty()) bar.chords.back().beats++; }
                        else bar.chords.push_back({t, 1});
                    }
                    int total = 0; for (auto& c : bar.chords) total += c.beats;
                    if (total < beatsPerBar && !bar.chords.empty()) bar.chords.back().beats += beatsPerBar - total;
                    while (total > beatsPerBar && !bar.chords.empty()) {
                        int over = total - beatsPerBar;
                        if (bar.chords.back().beats > over) { bar.chords.back().beats -= over; total -= over; }
                        else { total -= bar.chords.back().beats; bar.chords.pop_back(); }
                    }
                } else {
                    auto dist = distributeBeats((int)toks.size(), beatsPerBar);
                    for (size_t i = 0; i < toks.size(); i++) bar.chords.push_back({toks[i], dist[i]});
                }
            }
            if (haveSection) { bar.section = section; haveSection = false; }
            bars.push_back(bar);
            prevIdx = (int)bars.size() - 1;
        }
    }
    return bars;
}

std::string chartToText(const std::vector<Bar>& bars, int beatsPerBar, int perLine) {
    std::vector<std::string> lines, cur;
    auto flush = [&]() {
        if (cur.empty()) return;
        std::string l = "| ";
        for (size_t i = 0; i < cur.size(); i++) { if (i) l += " | "; l += cur[i]; }
        lines.push_back(l + " |"); cur.clear();
    };
    for (const Bar& bar : bars) {
        if (!bar.section.empty()) { flush(); lines.push_back("[" + bar.section + "]"); }
        auto even = distributeBeats((int)bar.chords.size(), beatsPerBar);
        bool isEven = true;
        for (size_t i = 0; i < bar.chords.size(); i++) if (bar.chords[i].beats != even[i]) isEven = false;
        std::string s;
        for (size_t i = 0; i < bar.chords.size(); i++) {
            if (i) s += " ";
            s += bar.chords[i].symbol;
            if (!isEven) for (int k = 1; k < bar.chords[i].beats; k++) s += " _";
        }
        cur.push_back(s);
        if ((int)cur.size() >= perLine) flush();
    }
    flush();
    std::string out;
    for (size_t i = 0; i < lines.size(); i++) { if (i) out += "\n"; out += lines[i]; }
    return out;
}

std::vector<Bar> transposeBars(const std::vector<Bar>& bars, int semis, bool flats) {
    std::vector<Bar> out = bars;
    for (Bar& b : out) for (ChordCell& c : b.chords) c.symbol = parseChord(c.symbol).transposed(semis, flats);
    return out;
}

std::vector<BeatSlot> flattenBars(const std::vector<Bar>& bars, int beatsPerBar) {
    std::vector<BeatSlot> slots;
    for (int bi = 0; bi < (int)bars.size(); bi++) {
        const Bar& bar = bars[bi];
        int beat = 0;
        for (int ci = 0; ci < (int)bar.chords.size(); ci++)
            for (int k = 0; k < bar.chords[ci].beats; k++)
                slots.push_back({bi, ci, beat++, bar.chords[ci].symbol, k, bar.chords[ci].beats});
        while (beat < beatsPerBar) {
            int ci = std::max(0, (int)bar.chords.size() - 1);
            slots.push_back({bi, ci, beat++, bar.chords.empty() ? "NC" : bar.chords.back().symbol, 0, 1});
        }
    }
    return slots;
}

} // namespace giro
