// Giro core — chart text format and bar/beat model.
#pragma once
#include <string>
#include <vector>

namespace giro {

struct ChordCell { std::string symbol; int beats = 1; bool operator==(const ChordCell& o) const { return symbol == o.symbol && beats == o.beats; } };
struct Bar { std::vector<ChordCell> chords; std::string section; bool operator==(const Bar& o) const { return chords == o.chords && section == o.section; } };

struct BeatSlot {
    int barIndex, chordIndex, beatInBar;
    std::string symbol;
    int chordBeat, chordBeats;   // position inside the chord, chord length
};

/// Text format: bars separated by "|"; chords in a bar separated by spaces (split evenly);
/// "_" holds the previous chord one more beat (then every token is one beat); "%" repeats the
/// previous bar; "NC" = no chord; "[A]" or "A:" at line start opens a section; "x2" is ignored.
std::vector<Bar> parseChart(const std::string& text, int beatsPerBar);
std::string chartToText(const std::vector<Bar>& bars, int beatsPerBar, int perLine = 4);
std::vector<Bar> transposeBars(const std::vector<Bar>& bars, int semis, bool flats);
std::vector<BeatSlot> flattenBars(const std::vector<Bar>& bars, int beatsPerBar);
std::vector<int> distributeBeats(int n, int beatsPerBar);

} // namespace giro
