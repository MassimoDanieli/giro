#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../core/chart.h"

/// The lead sheet: 4 bars per row, section labels, bar numbers, beat dots, loop shading,
/// and the chord being played drawn white on red.
class ChartComponent : public juce::Component {
public:
    std::function<void(int barIndex)> onBarClicked;

    void setBars(std::vector<giro::Bar> bars, int beatsPerBar);
    void setPosition(int bar, int chord, int beat);     // -1 = nothing playing
    void setLoop(int a, int b);                         // -1 = none
    int preferredHeight(int width) const;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    struct Row { std::vector<int> bars; };
    std::vector<giro::Bar> bars;
    std::vector<Row> rows;
    int bpb = 4;
    int posBar = -1, posChord = -1, posBeat = -1;
    int loopA = -1, loopB = -1;
    std::vector<juce::Rectangle<int>> barRects;
    static constexpr int kRowHeight = 86, kPad = 16, kBarsPerRow = 4;
    void layoutRows();
};
