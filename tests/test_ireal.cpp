#include "../core/ireal.h"
#include "../core/chart.h"
#include <cstdio>
#include <fstream>
#include <sstream>
using namespace giro;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { fails++; std::printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { fails++; std::printf("FAIL %d: %s == %s\n", __LINE__, #a, #b); } } while (0)
#define CHECK_EQS(a, b) do { std::string _a = (a), _b = (b); if (_a != _b) { fails++; std::printf("FAIL %d: %s\n   got:  %s\n   want: %s\n", __LINE__, #a, _a.c_str(), _b.c_str()); } } while (0)
static std::string slurp(const char* p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
static std::string join(const std::vector<Bar>& bars) { std::string s; for (auto& b : bars) { if (!s.empty()) s += " | "; for (size_t i = 0; i < b.chords.size(); i++) { if (i) s += " "; s += b.chords[i].symbol; if (b.chords[i].beats != (int)(4 / b.chords.size()) ) s += "(" + std::to_string(b.chords[i].beats) + ")"; } } return s; }

int main(int argc, char** argv) {
    CHECK_EQ(irealChordToGiro("C^7"), std::string("Cmaj7"));
    CHECK_EQ(irealChordToGiro("Eh7"), std::string("Em7b5"));
    CHECK_EQ(irealChordToGiro("Eh"), std::string("Em7b5"));
    CHECK_EQ(irealChordToGiro("A-7"), std::string("Am7"));
    CHECK_EQ(irealChordToGiro("D-"), std::string("Dm"));
    CHECK_EQ(irealChordToGiro("Bb7b9"), std::string("Bb7b9"));
    CHECK_EQ(irealChordToGiro("G7sus"), std::string("G7sus4"));
    CHECK_EQ(irealChordToGiro("Ao7"), std::string("Adim7"));
    CHECK_EQ(irealChordToGiro("Ao"), std::string("Adim"));
    CHECK_EQ(irealChordToGiro("C+"), std::string("Caug"));
    CHECK_EQ(irealChordToGiro("G-7/Bb"), std::string("Gm7/Bb"));
    CHECK_EQ(irealChordToGiro("F^7(A-7)"), std::string("Fmaj7"));
    CHECK_EQ(irealChordToGiro("W/C"), std::string("C"));
    CHECK_EQ(irealChordToGiro("C69"), std::string("C69"));
    CHECK_EQ(irealChordToGiro("D7alt"), std::string("D7alt"));
    CHECK_EQ(irealChordToGiro("C2"), std::string("Cadd9"));
    CHECK_EQ(irealChordToGiro("n"), std::string("NC"));
    CHECK_EQ(std::string(irealStyleToGiro("Bossa Nova")), std::string("bossa"));
    CHECK_EQ(std::string(irealStyleToGiro("Medium Swing")), std::string("swing"));
    CHECK_EQ(std::string(irealStyleToGiro("Slow Blues")), std::string("shuffle"));

    // vector 1 (from pyRealParser's tests): repeat with endings, x, r, codas text, N3
    auto v1 = parseIReal(slurp("vec1.txt"));
    CHECK(v1.error.empty());
    CHECK_EQ(v1.songs.size(), (size_t)1);
    if (!v1.songs.empty()) {
        auto& s = v1.songs[0];
        CHECK_EQ(s.title, std::string("Test"));
        CHECK_EQ(s.composer, std::string("Testy McTest"));
        CHECK_EQ(s.style, std::string("Up Tempo Swing"));
        CHECK_EQ(s.key, std::string("Eb"));
        CHECK_EQ(s.beatsPerBar, 4);
        std::printf("V1 raw: %s\n", s.raw.c_str());
        std::printf("V1 (%zu bars): %s\n", s.bars.size(), join(s.bars).c_str());
        // reference flattening: 16 bars of A with first ending, then A again with second ending (Eb6 | Eb6), B part...
        CHECK_EQ(s.bars[0].chords[0].symbol, std::string("Ebmaj7"));
        CHECK_EQ(s.bars[0].section, std::string("A"));
        CHECK_EQ(s.bars[3].chords.size(), (size_t)2);
        CHECK_EQ(s.bars[3].chords[0].beats, 2);
        CHECK_EQ(s.bars[10].chords[0].symbol, std::string("Gm7/Bb"));
        CHECK_EQ(s.bars[14].chords[0].symbol, std::string("Gm7"));       // N1 bar: Gm7 Gb7
        CHECK_EQ(s.bars[16].chords[0].symbol, std::string("Ebmaj7"));    // second pass starts
        CHECK_EQ(s.bars[30].chords[0].symbol, std::string("Eb6"));       // N2
        CHECK_EQ(s.bars[31].chords[0].symbol, std::string("Eb6"));       // x
        CHECK_EQ(s.bars[32].chords[0].symbol, std::string("Bbm7"));      // *B
        CHECK_EQ(s.bars[32].section, std::string("B"));
        CHECK_EQ(s.bars[34].chords[0].symbol, std::string("Bbm7"));      // r
        CHECK_EQ(s.bars[35].chords[0].symbol, std::string("Eb7"));
        CHECK_EQ(s.bars[36].chords[0].symbol, std::string("Bbm7"));      // r
        CHECK_EQ(s.bars[41].chords.size(), (size_t)2); CHECK_EQ(s.bars[41].chords[0].beats, 2);   // sBb-7,Eb7
        CHECK_EQ(s.bars[42].chords[0].symbol, std::string("Abmaj7"));    // lAb^7
        CHECK_EQ(s.bars[44].chords[0].symbol, std::string("Abmaj7"));    // x x
        CHECK_EQ(s.bars[46].chords[0].symbol, std::string("Bb7sus4"));
    }
    // vector 2: '}' without '{' (repeat from the section start), small chords with commas
    auto v2 = parseIReal(slurp("vec2.txt"));
    CHECK_EQ(v2.songs.size(), (size_t)1);
    if (!v2.songs.empty()) {
        auto& s = v2.songs[0];
        CHECK_EQ(s.title, std::string("As Long As I Live"));
        CHECK_EQ(s.composer, std::string("Harold Arlen"));
        CHECK_EQ(s.key, std::string("F"));
        std::printf("V2 (%zu bars): %s\n", s.bars.size(), join(s.bars).c_str());
        std::string expect = "Fmaj7 | Em7b5 A7b9 | D7 | Am7 D7 | G7 | Gm7 C7 | Fmaj7 Dm7 | Gm7 C7 | "
                             "Fmaj7 | Em7b5 A7b9 | D7 | Am7 D7 | G7 | Gm7 C7 | Fmaj7 Bb7 | F6 | "
                             "Cm7 F7 | Bbmaj7 | Bbm7 Eb7 | Fmaj7(2) Em7b5 A7 | Dm7 | G7 | Gm7 | C7 | "
                             "Fmaj7 | Em7b5 A7b9 | D7 | Am7 D7 | G7 | Gm7 C7 | Fmaj7 Bb7 | F6";
        CHECK_EQS(join(s.bars), expect);
        CHECK_EQ(s.bars.size(), (size_t)32);
        CHECK_EQ(s.bars[6].section, std::string("1."));
        CHECK_EQ(s.bars[14].section, std::string("2."));
        CHECK_EQ(s.bars[16].section, std::string("B"));
    }
    // HTML playlist wrapper + percent-encoding + two links in one text
    std::string html = "<html><body><a href=\"" + slurp("vec2.txt") + "\">As Long</a> <a href='" + slurp("vec1.txt") + "'>Test</a></body></html>";
    auto v3 = parseIReal(html);
    CHECK_EQ(v3.songs.size(), (size_t)2);
    // old irealbook:// format, unobfuscated
    auto v4 = parseIReal("irealbook://My%20Tune=Me=Medium%20Swing=G=n=*A%7BT44G%5E7%20%20%20%7CE-7%20A7%7CD7%20%20%20%7CG6%20%20%20%7D");
    CHECK_EQ(v4.songs.size(), (size_t)1);
    if (!v4.songs.empty()) { std::printf("V4: %s\n", join(v4.songs[0].bars).c_str()); CHECK_EQ(join(v4.songs[0].bars), std::string("Gmaj7 | Em7 A7 | D7 | G6 | Gmaj7 | Em7 A7 | D7 | G6")); CHECK_EQ(v4.songs[0].key, std::string("G")); }
    CHECK(!parseIReal("ciao").error.empty());
    if (argc > 1) { auto v = parseIReal(slurp(argv[1])); std::printf("file: %zu songs, playlist '%s' %s\n", v.songs.size(), v.playlist.c_str(), v.error.c_str()); for (auto& s : v.songs) std::printf("  %s [%s, %s, %d/4, %.0f] %zu bars\n    %s\n", s.title.c_str(), s.key.c_str(), s.style.c_str(), s.beatsPerBar, s.tempo, s.bars.size(), chartToText(s.bars, s.beatsPerBar).c_str()); }
    if (fails) { std::printf("%d FAILURES\n", fails); return 1; }
    std::printf("IREAL OK\n"); return 0;
}
