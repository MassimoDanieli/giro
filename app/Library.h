#pragma once
#include <juce_core/juce_core.h>
#include "../core/chart.h"
#include "../core/theory.h"
#include "../core/patterns.h"

struct Song {
    juce::String id, title, key = "C";
    double tempo = 120;
    giro::Style style = giro::Style::Swing;
    int beatsPerBar = 4;
    juce::String text;

    std::vector<giro::Bar> bars() const { return giro::parseChart(text.toStdString(), beatsPerBar); }
    bool prefersFlats() const { return giro::keyPrefersFlats(key.toStdString()); }

    /// Rewrites key and chart text by `semis` semitones (so editor, chart and player always agree).
    void transpose(int semis);

    juce::var toVar() const;
    static Song fromVar(const juce::var& v);
    static juce::String newId();
};

class Library {
public:
    Library();
    std::vector<Song> songs;
    juce::String selectedId;

    static juce::File file();
    void load();
    void save() const;
    int selectedIndex() const;
    Song& selected();
    void add(const Song& s);
    void removeSelected();
    static std::vector<Song> samples();

    static const juce::StringArray& keys();
};
