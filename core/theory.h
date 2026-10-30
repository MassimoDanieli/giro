// Giro core — chord symbols, pitch spelling, transposition. Plain C++17, no dependencies.
#pragma once
#include <string>
#include <vector>

namespace giro {

std::string pcName(int pc, bool flats);
int noteToPc(const std::string& name);          // "F#", "Bb", "C" -> 0..11, or -1 if not a note
bool keyPrefersFlats(const std::string& key);   // "F", "Bb", "Dm" ...
std::string transposeKey(const std::string& key, int semis);

struct Chord {
    std::string symbol;        // as written
    bool nc = false;           // "NC" / "N.C."
    bool valid = true;
    std::string root;          // "Bb"
    int rootPc = 0;
    std::string quality;       // canonical: "maj", "m7", "7b9" ...
    std::string qualityText;   // as typed: "Δ7", "-7" ...
    std::vector<int> intervals;
    std::string bass;          // slash bass, "" if none
    int bassPc = -1;

    std::string transposed(int semis, bool flats) const;
    std::string pretty() const;   // CΔ7, Dø7, E♭7♭9 ...
};

Chord parseChord(const std::string& symbol);
const std::vector<int>* intervalsFor(const std::string& canonicalQuality);
std::string displayQuality(const std::string& canonicalQuality);

} // namespace giro
