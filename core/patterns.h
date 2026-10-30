// Giro core — turns a chart into a MIDI event list (walking bass, comping, drums) for a given style.
#pragma once
#include <string>
#include <vector>
#include "chart.h"

namespace giro {

enum class Style { Swing, Bossa, Straight, Ballad, Shuffle };
const char* styleName(Style s);          // "swing", "bossa" ...
const char* styleLabel(Style s);         // "Swing", "Bossa", "Straight 8" ...
Style styleFromName(const std::string& n);
bool styleIsSwung(Style s);

enum class Part { Drums, Bass, Comp };

struct Event {
    double beat;      // absolute position in beats (count-in included)
    Part part;
    int note;         // MIDI note (GM drum note for Part::Drums)
    double len;       // in beats
    int vel;          // 1..127
};

struct SequenceSpec {
    Style style = Style::Swing;
    int beatsPerBar = 4;
    int loopStartBar = -1, loopEndBar = -1;   // inclusive, -1 = whole chart
    int choruses = 3;                         // 0 = "infinite" (capped)
    bool countIn = true;
};

struct Sequence {
    std::vector<Event> events;
    std::vector<BeatSlot> slots;    // whole chart, flattened
    int rangeStart = 0;             // first slot index of the loop range
    int rangeBeats = 0;             // beats in one chorus of the range
    int totalBeats = 0;             // beats of music (count-in excluded)
    int countInBeats = 0;
    double lengthBeats() const { return countInBeats + totalBeats + 1; }

    /// Maps an absolute beat position to a chart slot; returns false during count-in / after the end.
    bool slotAt(double beat, const BeatSlot*& slot, int& chorus) const;
};

/// GM program numbers the app should load for a style (bass, comp).
void gmPrograms(Style s, int& bassProgram, int& compProgram);

Sequence buildSequence(const std::vector<Bar>& bars, const SequenceSpec& spec);

/// GM drum notes used by the generator.
namespace GM { enum : int { Kick = 36, Snare = 38, Rim = 37, Hat = 42, HatOpen = 46, Pedal = 44, Ride = 51, ClickHi = 76, ClickLo = 77 }; }

} // namespace giro
