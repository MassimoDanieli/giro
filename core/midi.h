// Giro core — Standard MIDI File reader and chord detection.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "chart.h"

namespace giro {

struct MidiNote { double start, end; int pitch, vel, channel; };   // times in quarter-note beats

struct MidiFile {
    int format = 0, ppq = 480;
    double bpm = 120;
    int num = 4, den = 4;
    std::string title, keySig;       // keySig: "F", "Dm", "" if absent
    std::vector<MidiNote> notes;     // drum channel (9) excluded
};

bool parseMidi(const std::vector<uint8_t>& bytes, MidiFile& out, std::string& error);

struct DetectedSong {
    std::string title, key;
    double tempo = 120;
    int beatsPerBar = 4;
    std::vector<Bar> bars;
};

DetectedSong detectChords(const MidiFile& midi);

} // namespace giro
