#include "ChartComponent.h"
#include "../core/theory.h"

using namespace giro;

void ChartComponent::setBars(std::vector<Bar> b, int beatsPerBar) {
    bars = std::move(b); bpb = juce::jmax(1, beatsPerBar);
    layoutRows();
    resized();
    repaint();
}

void ChartComponent::layoutRows() {
    rows.clear();
    Row cur;
    for (int i = 0; i < (int)bars.size(); i++) {
        bool newSection = !bars[i].section.empty() && bars[i].section.back() != '.';   // "1." / "2." endings stay in the row
        if ((int)cur.bars.size() >= kBarsPerRow || (newSection && !cur.bars.empty())) { rows.push_back(cur); cur = Row(); }
        cur.bars.push_back(i);
    }
    if (!cur.bars.empty()) rows.push_back(cur);
}

int ChartComponent::preferredHeight(int) const { return kPad * 2 + juce::jmax(1, (int)rows.size()) * kRowHeight + 10; }

void ChartComponent::setPosition(int bar, int chord, int beat) {
    if (bar == posBar && chord == posChord && beat == posBeat) return;
    posBar = bar; posChord = chord; posBeat = beat;
    repaint();
}

void ChartComponent::setLoop(int a, int b) { loopA = a; loopB = b; repaint(); }

void ChartComponent::resized() {
    barRects.assign(bars.size(), {});
    int w = juce::jmax(100, getWidth() - kPad * 2);
    int colW = w / kBarsPerRow;
    for (int r = 0; r < (int)rows.size(); r++)
        for (int k = 0; k < (int)rows[r].bars.size(); k++)
            barRects[(size_t)rows[r].bars[(size_t)k]] = {kPad + k * colW, kPad + r * kRowHeight, colW, kRowHeight};
}

void ChartComponent::mouseUp(const juce::MouseEvent& e) {
    for (int i = 0; i < (int)barRects.size(); i++)
        if (barRects[(size_t)i].contains(e.getPosition())) { if (onBarClicked) onBarClicked(i); return; }
}

void ChartComponent::paint(juce::Graphics& g) {
    const auto paper = juce::Colour(0xfff3efe4), ink = juce::Colour(0xff1b1a17), faint = juce::Colour(0xffb9b2a0), red = juce::Colour(0xffd42a2a), loopCol = juce::Colour(0x30c08a1c);
    g.fillAll(paper);
    g.setColour(ink);
    if (bars.empty()) {
        g.setFont(juce::Font(juce::FontOptions(16.0f)));
        g.setColour(faint);
        g.drawText(juce::CharPointer_UTF8("Scrivi gli accordi nel box qui sotto o importa un file MIDI."), getLocalBounds(), juce::Justification::centred);
        return;
    }
    juce::Font chordFont(juce::FontOptions(24.0f, juce::Font::bold));
    juce::Font small(juce::FontOptions(10.0f));
    juce::Font secFont(juce::FontOptions(12.0f, juce::Font::bold));

    for (int i = 0; i < (int)bars.size(); i++) {
        const Bar& bar = bars[(size_t)i];
        auto r = barRects[(size_t)i];
        if (r.isEmpty()) continue;
        bool isCur = i == posBar;
        if (loopA >= 0 && i >= loopA && i <= loopB) { g.setColour(loopCol); g.fillRect(r); }
        // bar lines
        g.setColour(ink);
        g.fillRect(r.getX(), r.getY() + 4, 2, r.getHeight() - 8);
        bool lastInRow = false;
        for (auto& row : rows) if (!row.bars.empty() && row.bars.back() == i) lastInRow = true;
        if (lastInRow) g.fillRect(r.getRight() - 2, r.getY() + 4, 2, r.getHeight() - 8);
        // section
        if (!bar.section.empty()) {
            g.setFont(secFont);
            int tw = juce::roundToInt(juce::GlyphArrangement::getStringWidth(secFont, juce::String::fromUTF8(bar.section.c_str())) + 12);
            juce::Rectangle<int> sr(r.getX() - 1, r.getY() + 4, tw, 16);
            g.setColour(ink); g.fillRoundedRectangle(sr.toFloat(), 3.0f);
            g.setColour(paper); g.drawText(juce::String::fromUTF8(bar.section.c_str()), sr, juce::Justification::centred);
        }
        // bar number
        g.setFont(small); g.setColour(faint);
        g.drawText(juce::String(i + 1), r.getX(), r.getY() + 5, r.getWidth() - 6, 12, juce::Justification::centredRight);
        // chords
        int total = 0; for (auto& c : bar.chords) total += c.beats;
        total = juce::jmax(1, total);
        int x = r.getX() + 8, avail = r.getWidth() - 16;
        int top = r.getY() + 26, h = 36;
        g.setFont(chordFont);
        for (int ci = 0; ci < (int)bar.chords.size(); ci++) {
            const ChordCell& cell = bar.chords[(size_t)ci];
            int w = avail * cell.beats / total;
            juce::Rectangle<int> cr(x, top, w - 3, h);
            Chord ch = parseChord(cell.symbol);
            bool now = isCur && ci == posChord;
            if (now) { g.setColour(red); g.fillRoundedRectangle(cr.toFloat(), 6.0f); g.setColour(juce::Colours::white); }
            else g.setColour(ch.valid ? ink : red);
            juce::String txt = juce::String::fromUTF8(ch.pretty().c_str());
            g.setFont(chordFont);
            float tw = juce::GlyphArrangement::getStringWidth(chordFont, txt);
            if (tw > cr.getWidth() - 10) g.setFont(juce::Font(juce::FontOptions(juce::jmax(12.0f, 24.0f * (cr.getWidth() - 10) / tw), juce::Font::bold)));
            g.drawText(txt, cr.reduced(5, 0), juce::Justification::centredLeft, false);
            x += w;
        }
        // beat dots
        for (int k = 0; k < bpb; k++) {
            g.setColour(isCur && k == posBeat ? red : faint);
            g.fillEllipse((float)(r.getX() + 10 + k * 9), (float)(r.getBottom() - 12), 5.0f, 5.0f);
        }
        if (isCur) { g.setColour(red); g.fillRect(r.getX() + 2, r.getBottom() - 4, r.getWidth() - 4, 2); }
    }
}
