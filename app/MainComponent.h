#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Library.h"
#include "Engine.h"
#include "ChartComponent.h"

class MainComponent : public juce::AudioAppComponent, public juce::FileDragAndDropTarget, private juce::Timer, private juce::ListBoxModel {
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override { engine.prepareToPlay(samplesPerBlockExpected, sampleRate); }
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override { engine.getNextAudioBlock(info); }
    void releaseResources() override { engine.releaseResources(); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // actions reachable from the menu
    void togglePlay();
    void transpose(int semis);
    void newSong();
    void importMidi();
    void importIReal();                                  // opens the paste / file dialog
    void importIRealText(const juce::String& text);      // link or HTML playlist text
    void importMidiFile(const juce::File& f);

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    Library library;
    Engine engine;
    std::unique_ptr<juce::FileChooser> chooser;

    // sidebar
    juce::ListBox songList{"songs", this};
    juce::TextButton newBtn{"Nuovo"}, importBtn{"Importa MIDI"}, irealBtn{"Importa iReal Pro"}, dupBtn{"Duplica"}, delBtn{"Elimina"};
    std::unique_ptr<juce::Component> irealDialog;
    std::vector<int> sortedIdx;

    // meta
    juce::TextEditor titleEd;
    juce::ComboBox keyBox, timeBox, styleBox;
    juce::Label keyLbl{{}, juce::CharPointer_UTF8("Tonalit\u00E0")}, timeLbl{{}, "Tempo"}, styleLbl{{}, "Stile"};

    // transport
    juce::TextButton playBtn{"Play"}, trDown{"-"}, trUp{"+"}, clearLoopBtn{"Togli loop"};
    juce::Slider tempoSl, chorusSl, volBass, volComp, volDrums;
    juce::Label tempoLbl{{}, "BPM"}, trLbl{{}, "Trasp."}, trVal, chorusLbl{{}, "Giri"}, vbLbl{{}, "Basso"}, vcLbl{{}, "Comp"}, vdLbl{{}, "Batt."}, statusLbl;
    juce::ToggleButton countInBtn{"Count-in"};

    // chart + editor
    juce::Viewport chartView;
    ChartComponent chart;
    juce::TextEditor textEd;
    juce::Label helpLbl;

    int loopA = -1, loopB = -1, loopAnchor = -1;
    bool deleteArmed = false, suppressTextCallback = false;

    void timerCallback() override;
    int getNumRows() override { return (int)sortedIdx.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
    void selectedRowsChanged(int lastRowSelected) override;

    void rebuildSongList();
    void loadSelectedIntoUI();
    void refreshChart();
    void setStatus(const juce::String& s) { statusLbl.setText(s, juce::dontSendNotification); }
    void restartIfPlaying();
    void setKeyFromBox();
    void barClicked(int i);
    static void styleLabel(juce::Label& l);
    static void miniSlider(juce::Slider& s);
};
