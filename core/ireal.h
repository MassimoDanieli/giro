// Giro core — import of iReal Pro links (irealb:// and the older irealbook://), including
// playlist HTML exports that embed such links.
#pragma once
#include <string>
#include <vector>
#include "chart.h"

namespace giro {

struct IRealSong {
    std::string title, composer, style, key;   // key as "F", "Dm"
    double tempo = 0;                          // 0 = not given in the link
    int beatsPerBar = 4;
    std::vector<Bar> bars;                     // repeats, endings and codas expanded
    std::string raw;                           // unscrambled chord string, for debugging
};

struct IRealImport {
    std::string playlist;                      // playlist name, if the link carries one
    std::vector<IRealSong> songs;
    std::string error;
};

/// Accepts a bare link, a percent-encoded link, or any text (e.g. an HTML playlist export)
/// that contains one or more irealb:// / irealbook:// links.
IRealImport parseIReal(const std::string& text);

/// Helpers exposed for tests.
std::string irealUnscramble(const std::string& scrambled);
std::string irealPercentDecode(const std::string& s);
std::string irealChordToGiro(const std::string& irealChord);   // "C^7" -> "Cmaj7", "Eh7" -> "Em7b5"
double irealDefaultTempo(const std::string& style);
const char* irealStyleToGiro(const std::string& style);       // "swing", "bossa", ...

} // namespace giro
