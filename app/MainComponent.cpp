#include "MainComponent.h"
#include "../core/theory.h"
#include "../core/midi.h"
#include "../core/ireal.h"

using namespace giro;

void MainComponent::styleLabel(juce::Label& l) { l.setFont(juce::Font(juce::FontOptions(11.0f))); l.setColour(juce::Label::textColourId, juce::Colours::grey); l.setJustificationType(juce::Justification::centredRight); }
void MainComponent::miniSlider(juce::Slider& s) { s.setSliderStyle(juce::Slider::LinearHorizontal); s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); s.setRange(0, 1, 0.01); }

MainComponent::MainComponent() {
    setSize(1180, 800);
    setWantsKeyboardFocus(true);

    // --- sidebar
    addAndMakeVisible(songList);
    songList.setRowHeight(26);
    for (auto* b : {&newBtn, &importBtn, &irealBtn, &dupBtn, &delBtn}) addAndMakeVisible(b);
    newBtn.onClick = [this] { newSong(); };
    importBtn.onClick = [this] { importMidi(); };
    irealBtn.onClick = [this] { importIReal(); };
    dupBtn.onClick = [this] { engine.stop(); Song s = library.selected(); s.id = Song::newId(); s.title += " (2)"; library.add(s); rebuildSongList(); loadSelectedIntoUI(); };
    delBtn.onClick = [this] {
        if (!deleteArmed) { deleteArmed = true; delBtn.setButtonText("Confermi?"); delBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred); setStatus("Clicca di nuovo per eliminare."); return; }
        engine.stop(); library.removeSelected(); deleteArmed = false; delBtn.setButtonText("Elimina"); delBtn.removeColour(juce::TextButton::buttonColourId);
        rebuildSongList(); loadSelectedIntoUI(); setStatus("Brano eliminato.");
    };

    // --- meta
    addAndMakeVisible(titleEd);
    titleEd.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    titleEd.setTextToShowWhenEmpty("Titolo", juce::Colours::grey);
    titleEd.onTextChange = [this] { library.selected().title = titleEd.getText(); library.save(); songList.repaint(); };
    for (auto* l : {&keyLbl, &timeLbl, &styleLbl}) { addAndMakeVisible(l); styleLabel(*l); }
    addAndMakeVisible(keyBox); addAndMakeVisible(timeBox); addAndMakeVisible(styleBox);
    for (int i = 0; i < Library::keys().size(); i++) keyBox.addItem(Library::keys()[i], i + 1);
    keyBox.onChange = [this] { setKeyFromBox(); };
    timeBox.addItem("2/4", 2); timeBox.addItem("3/4", 3); timeBox.addItem("4/4", 4); timeBox.addItem("5/4", 5); timeBox.addItem("6/4", 6); timeBox.addItem("7/4", 7);
    timeBox.onChange = [this] { library.selected().beatsPerBar = timeBox.getSelectedId(); library.save(); refreshChart(); restartIfPlaying(); };
    for (Style st : {Style::Swing, Style::Bossa, Style::Straight, Style::Ballad, Style::Shuffle}) styleBox.addItem(giro::styleLabel(st), (int)st + 1);
    styleBox.onChange = [this] { library.selected().style = (Style)(styleBox.getSelectedId() - 1); library.save(); restartIfPlaying(); };

    // --- transport
    addAndMakeVisible(playBtn);
    playBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2f6fdb));
    playBtn.onClick = [this] { togglePlay(); };
    addAndMakeVisible(tempoSl); tempoSl.setRange(40, 300, 1); tempoSl.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 22);
    tempoSl.onValueChange = [this] { library.selected().tempo = tempoSl.getValue(); engine.setTempo(tempoSl.getValue()); };
    tempoSl.onDragEnd = [this] { library.save(); };
    addAndMakeVisible(trDown); addAndMakeVisible(trUp); addAndMakeVisible(trVal);
    trDown.onClick = [this] { transpose(-1); }; trUp.onClick = [this] { transpose(1); };
    trVal.setJustificationType(juce::Justification::centred); trVal.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    addAndMakeVisible(chorusSl); chorusSl.setRange(0, 20, 1); chorusSl.setValue(3); chorusSl.setSliderStyle(juce::Slider::IncDecButtons);
    chorusSl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 30, 22);
    chorusSl.textFromValueFunction = [](double v) { return v == 0 ? juce::String(juce::CharPointer_UTF8("∞")) : juce::String((int)v); };
    addAndMakeVisible(countInBtn); countInBtn.setToggleState(true, juce::dontSendNotification);
    for (auto* s : {&volBass, &volComp, &volDrums}) { addAndMakeVisible(s); miniSlider(*s); }
    volBass.setValue(0.9); volComp.setValue(0.6); volDrums.setValue(0.7);
    volBass.onValueChange = [this] { engine.setVolume(Part::Bass, (float)volBass.getValue()); };
    volComp.onValueChange = [this] { engine.setVolume(Part::Comp, (float)volComp.getValue()); };
    volDrums.onValueChange = [this] { engine.setVolume(Part::Drums, (float)volDrums.getValue()); };
    for (auto* l : {&tempoLbl, &trLbl, &chorusLbl, &vbLbl, &vcLbl, &vdLbl}) { addAndMakeVisible(l); styleLabel(*l); }
    addAndMakeVisible(clearLoopBtn); clearLoopBtn.setVisible(false);
    clearLoopBtn.onClick = [this] { loopA = loopB = loopAnchor = -1; chart.setLoop(-1, -1); clearLoopBtn.setVisible(false); restartIfPlaying(); };
    addAndMakeVisible(statusLbl); statusLbl.setFont(juce::Font(juce::FontOptions(12.0f))); statusLbl.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    // --- chart + editor
    addAndMakeVisible(chartView);
    chartView.setViewedComponent(&chart, false);
    chartView.setScrollBarsShown(true, false);
    chart.onBarClicked = [this](int i) { barClicked(i); };
    addAndMakeVisible(textEd);
    textEd.setMultiLine(true, false); textEd.setReturnKeyStartsNewLine(true);
    textEd.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain)));
    textEd.onTextChange = [this] { if (suppressTextCallback) return; library.selected().text = textEd.getText(); library.save(); refreshChart(); };
    textEd.onFocusLost = [this] { restartIfPlaying(); };
    addAndMakeVisible(helpLbl);
    helpLbl.setFont(juce::Font(juce::FontOptions(11.5f))); helpLbl.setColour(juce::Label::textColourId, juce::Colours::grey);
    helpLbl.setText(juce::CharPointer_UTF8("Battute separate da |. Accordi nella stessa battuta separati da spazio (divisi in parti uguali); _ tiene l'accordo un battito in più (A7 _ _ Eb7 = 3+1). % ripete la battuta precedente, NC = nessun accordo, [A] a inizio riga apre una sezione. Clicca una battuta per l'inizio del loop, un'altra per la fine. Spazio = play/stop (quando non stai scrivendo), ⌘+ / ⌘− = trasposizione."), juce::dontSendNotification);

    rebuildSongList();
    loadSelectedIntoUI();
    setAudioChannels(0, 2);
    startTimerHz(30);
}

MainComponent::~MainComponent() { stopTimer(); shutdownAudio(); }

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff15171c));
    g.setColour(juce::Colour(0xff1d2026));
    g.fillRect(0, 0, 240, getHeight());
    g.fillRoundedRectangle(254.0f, 52.0f, (float)(getWidth() - 268), 84.0f, 8.0f);
    g.setColour(juce::Colour(0xff343945));
    g.drawVerticalLine(240, 0, (float)getHeight());
}

void MainComponent::resized() {
    auto r = getLocalBounds();
    auto side = r.removeFromLeft(240);
    auto btns = side.removeFromBottom(100).reduced(8, 6);
    auto row1 = btns.removeFromTop(28), row3 = btns.removeFromBottom(28), row2 = btns.reduced(0, 1);
    newBtn.setBounds(row1.removeFromLeft(row1.getWidth() / 2).reduced(2)); importBtn.setBounds(row1.reduced(2));
    irealBtn.setBounds(row2.reduced(2));
    dupBtn.setBounds(row3.removeFromLeft(row3.getWidth() / 2).reduced(2)); delBtn.setBounds(row3.reduced(2));
    songList.setBounds(side.reduced(0, 6));

    auto main = r.reduced(14, 10);
    auto meta = main.removeFromTop(34);
    styleBox.setBounds(meta.removeFromRight(130)); styleLbl.setBounds(meta.removeFromRight(40));
    timeBox.setBounds(meta.removeFromRight(70)); timeLbl.setBounds(meta.removeFromRight(50));
    keyBox.setBounds(meta.removeFromRight(80)); keyLbl.setBounds(meta.removeFromRight(60));
    titleEd.setBounds(meta.withTrimmedRight(10));

    main.removeFromTop(8);
    auto band = main.removeFromTop(84).reduced(10, 10);
    auto tr = band.removeFromTop(30);
    playBtn.setBounds(tr.removeFromLeft(80)); tr.removeFromLeft(14);
    tempoLbl.setBounds(tr.removeFromLeft(32)); tempoSl.setBounds(tr.removeFromLeft(200)); tr.removeFromLeft(14);
    trLbl.setBounds(tr.removeFromLeft(44)); trDown.setBounds(tr.removeFromLeft(28)); trVal.setBounds(tr.removeFromLeft(50)); trUp.setBounds(tr.removeFromLeft(28)); tr.removeFromLeft(14);
    chorusLbl.setBounds(tr.removeFromLeft(34)); chorusSl.setBounds(tr.removeFromLeft(96)); tr.removeFromLeft(8);
    countInBtn.setBounds(tr.removeFromLeft(90));
    statusLbl.setBounds(tr.removeFromRight(juce::jmax(100, tr.getWidth())));
    band.removeFromTop(4);
    auto tr2 = band.removeFromTop(30);
    tr2.removeFromLeft(94);
    vbLbl.setBounds(tr2.removeFromLeft(44)); volBass.setBounds(tr2.removeFromLeft(90)); tr2.removeFromLeft(10);
    vcLbl.setBounds(tr2.removeFromLeft(44)); volComp.setBounds(tr2.removeFromLeft(90)); tr2.removeFromLeft(10);
    vdLbl.setBounds(tr2.removeFromLeft(44)); volDrums.setBounds(tr2.removeFromLeft(90)); tr2.removeFromLeft(20);
    clearLoopBtn.setBounds(tr2.removeFromLeft(90));

    main.removeFromTop(10);
    helpLbl.setBounds(main.removeFromBottom(44));
    textEd.setBounds(main.removeFromBottom(130));
    main.removeFromBottom(8);
    chartView.setBounds(main);
    chart.setSize(chartView.getMaximumVisibleWidth(), juce::jmax(chartView.getMaximumVisibleHeight(), chart.preferredHeight(chartView.getMaximumVisibleWidth())));
}

// ---------------------------------------------------------------- list

void MainComponent::rebuildSongList() {
    sortedIdx.clear();
    for (int i = 0; i < (int)library.songs.size(); i++) sortedIdx.push_back(i);
    std::sort(sortedIdx.begin(), sortedIdx.end(), [this](int a, int b) { return library.songs[(size_t)a].title.compareIgnoreCase(library.songs[(size_t)b].title) < 0; });
    songList.updateContent();
    for (int row = 0; row < (int)sortedIdx.size(); row++)
        if (library.songs[(size_t)sortedIdx[(size_t)row]].id == library.selectedId) { songList.selectRow(row, true, true); break; }
    songList.repaint();
}

void MainComponent::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (row < 0 || row >= (int)sortedIdx.size()) return;
    const Song& s = library.songs[(size_t)sortedIdx[(size_t)row]];
    if (selected) { g.setColour(juce::Colour(0xff262a32)); g.fillRect(0, 0, w, h); g.setColour(juce::Colour(0xffd9b65c)); g.fillRect(0, 0, 3, h); }
    g.setColour(juce::Colours::white); g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawText(s.title.isEmpty() ? juce::String("-") : s.title, 12, 0, w - 60, h, juce::Justification::centredLeft, true);
    g.setColour(juce::Colours::grey); g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(s.key, w - 48, 0, 40, h, juce::Justification::centredRight);
}

void MainComponent::selectedRowsChanged(int row) {
    if (row < 0 || row >= (int)sortedIdx.size()) return;
    const juce::String id = library.songs[(size_t)sortedIdx[(size_t)row]].id;
    if (id == library.selectedId) return;
    engine.stop();
    library.selectedId = id; library.save();
    loopA = loopB = loopAnchor = -1; clearLoopBtn.setVisible(false);
    loadSelectedIntoUI();
}

void MainComponent::loadSelectedIntoUI() {
    Song& s = library.selected();
    titleEd.setText(s.title, false);
    keyBox.setSelectedId(Library::keys().indexOf(s.key) + 1, juce::dontSendNotification);
    timeBox.setSelectedId(s.beatsPerBar, juce::dontSendNotification);
    styleBox.setSelectedId((int)s.style + 1, juce::dontSendNotification);
    tempoSl.setValue(s.tempo, juce::dontSendNotification);
    suppressTextCallback = true; textEd.setText(s.text, false); suppressTextCallback = false;
    chart.setLoop(loopA, loopB);
    refreshChart();
}

void MainComponent::refreshChart() {
    Song& s = library.selected();
    trVal.setText(s.key, juce::dontSendNotification);
    chart.setBars(s.bars(), s.beatsPerBar);
    chart.setSize(chartView.getMaximumVisibleWidth(), juce::jmax(chartView.getMaximumVisibleHeight(), chart.preferredHeight(chartView.getMaximumVisibleWidth())));
}

// ---------------------------------------------------------------- actions

void MainComponent::togglePlay() {
    if (engine.isPlaying()) { engine.stop(); return; }
    Song& s = library.selected();
    auto bars = s.bars();
    if (bars.empty()) { setStatus("Nessuna battuta da suonare."); return; }
    SequenceSpec spec;
    spec.style = s.style; spec.beatsPerBar = s.beatsPerBar;
    spec.loopStartBar = loopA; spec.loopEndBar = loopB;
    spec.choruses = (int)chorusSl.getValue(); spec.countIn = countInBtn.getToggleState();
    int bp, cp; gmPrograms(s.style, bp, cp);
    engine.start(buildSequence(bars, spec), s.tempo, bp, cp);
}

void MainComponent::restartIfPlaying() { if (engine.isPlaying()) { engine.stop(); togglePlay(); } }

void MainComponent::transpose(int semis) {
    Song& s = library.selected();
    s.transpose(semis);
    library.save();
    keyBox.setSelectedId(Library::keys().indexOf(s.key) + 1, juce::dontSendNotification);
    suppressTextCallback = true; textEd.setText(s.text, false); suppressTextCallback = false;
    refreshChart(); songList.repaint();
    restartIfPlaying();
}

void MainComponent::setKeyFromBox() {
    juce::String newKey = keyBox.getText();
    Song& s = library.selected();
    auto rootOf = [](const juce::String& k) { auto b = k.endsWith("m") ? k.dropLastCharacters(1) : k; return noteToPc(b.toStdString()); };
    int d = ((rootOf(newKey) - rootOf(s.key)) % 12 + 12) % 12;
    if (d > 6) d -= 12;
    if (d != 0) transpose(d);
    s.key = newKey; library.save(); refreshChart(); songList.repaint();
}

void MainComponent::barClicked(int i) {
    if (loopAnchor >= 0 && loopA == loopB) { loopA = juce::jmin(loopAnchor, i); loopB = juce::jmax(loopAnchor, i); loopAnchor = -1; }
    else { loopA = loopB = loopAnchor = i; }
    chart.setLoop(loopA, loopB); clearLoopBtn.setVisible(true);
    restartIfPlaying();
}

void MainComponent::newSong() {
    engine.stop();
    Song s; s.id = Song::newId(); s.title = "Nuovo brano"; s.key = "C"; s.tempo = 120; s.style = Style::Swing; s.text = "| C | Am7 | Dm7 | G7 |";
    library.add(s); rebuildSongList(); loadSelectedIntoUI();
    titleEd.grabKeyboardFocus(); titleEd.selectAll();
}

void MainComponent::importMidi() {
    chooser = std::make_unique<juce::FileChooser>("Importa un file MIDI", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.mid;*.midi");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
        auto f = fc.getResult();
        if (f.existsAsFile()) importMidiFile(f);
    });
}

void MainComponent::importMidiFile(const juce::File& f) {
    juce::MemoryBlock mb; f.loadFileAsData(mb);
    std::vector<uint8_t> bytes((const uint8_t*)mb.getData(), (const uint8_t*)mb.getData() + mb.getSize());
    MidiFile mf; std::string err;
    if (!parseMidi(bytes, mf, err)) { setStatus(juce::String::fromUTF8(err.c_str())); return; }
    auto det = detectChords(mf);
    Song s; s.id = Song::newId();
    s.title = det.title.empty() ? f.getFileNameWithoutExtension() : juce::String::fromUTF8(det.title.c_str());
    s.key = det.key; s.tempo = juce::jlimit(40.0, 300.0, det.tempo); s.beatsPerBar = det.beatsPerBar; s.style = Style::Swing;
    s.text = chartToText(det.bars, det.beatsPerBar);
    engine.stop(); library.add(s); rebuildSongList(); loadSelectedIntoUI();
    setStatus("MIDI: " + juce::String((int)det.bars.size()) + " battute, " + juce::String((int)det.tempo) + juce::String(juce::CharPointer_UTF8(" BPM, tonalit\u00E0 ")) + s.key);
}

// ---------------------------------------------------------------- iReal Pro

namespace {
class IRealDialog : public juce::Component {
public:
    std::function<void(const juce::String&)> onImport;
    std::function<void()> onPickFile, onClose;
    IRealDialog() {
        addAndMakeVisible(info);
        info.setText(juce::CharPointer_UTF8("Incolla qui un link iReal Pro (irealb://\u2026), anche con pi\u00F9 brani o un'intera playlist. "
                                             "Da iReal Pro: Condividi \u2192 Copia link, oppure esporta la playlist in HTML e aprila con \u201CDa file\u2026\u201D. "
                                             "Puoi anche trascinare il file HTML sulla finestra di Giro."), juce::dontSendNotification);
        info.setJustificationType(juce::Justification::topLeft); info.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(text); text.setMultiLine(true, true); text.setReturnKeyStartsNewLine(true);
        text.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
        text.setTextToShowWhenEmpty("irealb://...", juce::Colours::grey);
        addAndMakeVisible(importBtn); addAndMakeVisible(fileBtn); addAndMakeVisible(cancelBtn);
        importBtn.onClick = [this] { if (onImport) onImport(text.getText()); };
        fileBtn.onClick = [this] { if (onPickFile) onPickFile(); };
        cancelBtn.onClick = [this] { if (onClose) onClose(); };
        setSize(560, 300);
    }
    void resized() override {
        auto r = getLocalBounds().reduced(14);
        info.setBounds(r.removeFromTop(64));
        auto b = r.removeFromBottom(30);
        cancelBtn.setBounds(b.removeFromLeft(90)); importBtn.setBounds(b.removeFromRight(110)); b.removeFromRight(8); fileBtn.setBounds(b.removeFromRight(110));
        r.removeFromBottom(10);
        text.setBounds(r);
    }
    void focusText() { text.grabKeyboardFocus(); }
private:
    juce::Label info;
    juce::TextEditor text;
    juce::TextButton importBtn{"Importa"}, fileBtn{juce::CharPointer_UTF8("Da file\u2026")}, cancelBtn{"Annulla"};
};
}

void MainComponent::importIReal() {
    auto* dlg = new IRealDialog();
    dlg->onImport = [this, dlg](const juce::String& t) { importIRealText(t); if (auto* w = dlg->findParentComponentOfClass<juce::DialogWindow>()) w->exitModalState(0); };
    dlg->onClose = [dlg] { if (auto* w = dlg->findParentComponentOfClass<juce::DialogWindow>()) w->exitModalState(0); };
    dlg->onPickFile = [this, dlg] {
        chooser = std::make_unique<juce::FileChooser>("Apri un export iReal Pro (HTML o testo)", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.html;*.htm;*.txt");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, dlg](const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (!f.existsAsFile()) return;
            importIRealText(f.loadFileAsString());
            if (auto* w = dlg->findParentComponentOfClass<juce::DialogWindow>()) w->exitModalState(0);
        });
    };
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(dlg);
    o.dialogTitle = "Importa da iReal Pro";
    o.dialogBackgroundColour = juce::Colour(0xff1d2026);
    o.escapeKeyTriggersCloseButton = true; o.useNativeTitleBar = true; o.resizable = false;
    o.launchAsync();
    dlg->focusText();
}

void MainComponent::importIRealText(const juce::String& text) {
    auto res = parseIReal(text.toStdString());
    if (res.songs.empty()) { setStatus(juce::String::fromUTF8(res.error.c_str())); return; }
    engine.stop();
    juce::String firstId;
    for (auto& is : res.songs) {
        Song s; s.id = Song::newId();
        s.title = juce::String::fromUTF8(is.title.c_str());
        if (!is.composer.empty()) s.title += juce::String::fromUTF8((" (" + is.composer + ")").c_str());
        s.key = juce::String(is.key);
        s.style = styleFromName(irealStyleToGiro(is.style));
        s.tempo = juce::jlimit(40.0, 300.0, is.tempo > 0 ? is.tempo : irealDefaultTempo(is.style));
        s.beatsPerBar = juce::jlimit(1, 12, is.beatsPerBar);
        s.text = chartToText(is.bars, s.beatsPerBar);
        library.songs.push_back(s);
        if (firstId.isEmpty()) firstId = s.id;
    }
    library.selectedId = firstId; library.save();
    rebuildSongList(); loadSelectedIntoUI();
    setStatus(juce::String("iReal: ") + juce::String((int)res.songs.size()) + (res.songs.size() == 1 ? " brano importato" : " brani importati") + (res.playlist.empty() ? "" : " da \u201C" + juce::String::fromUTF8(res.playlist.c_str()) + "\u201D"));
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files) {
    for (auto& f : files) { auto ext = juce::File(f).getFileExtension().toLowerCase(); if (ext == ".mid" || ext == ".midi" || ext == ".html" || ext == ".htm" || ext == ".txt") return true; }
    return false;
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int) {
    for (auto& path : files) {
        juce::File f(path); auto ext = f.getFileExtension().toLowerCase();
        if (ext == ".mid" || ext == ".midi") importMidiFile(f);
        else if (ext == ".html" || ext == ".htm" || ext == ".txt") importIRealText(f.loadFileAsString());
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
    auto* focus = getCurrentlyFocusedComponent();
    if (dynamic_cast<juce::TextEditor*>(focus) != nullptr) return false;
    if (key == juce::KeyPress::spaceKey) { togglePlay(); return true; }
    if (key.getTextCharacter() == '+' || key.getTextCharacter() == '=') { transpose(1); return true; }
    if (key.getTextCharacter() == '-') { transpose(-1); return true; }
    return false;
}

// ---------------------------------------------------------------- timer

void MainComponent::timerCallback() {
    static bool wasPlaying = false;
    bool playing = engine.isPlaying();
    if (playing != wasPlaying) {
        playBtn.setButtonText(playing ? "Stop" : "Play");
        playBtn.setColour(juce::TextButton::buttonColourId, playing ? juce::Colour(0xffd42a2a) : juce::Colour(0xff2f6fdb));
        if (!playing) { chart.setPosition(-1, -1, -1); setStatus(""); }
        wasPlaying = playing;
    }
    if (!playing) return;
    // the engine holds the sequence; we only need a snapshot of position
    double beat = engine.currentBeat();
    const Song& s = library.selected();
    SequenceSpec spec; spec.style = s.style; spec.beatsPerBar = s.beatsPerBar; spec.loopStartBar = loopA; spec.loopEndBar = loopB;
    spec.choruses = (int)chorusSl.getValue(); spec.countIn = countInBtn.getToggleState();
    static Sequence cached; static juce::String cachedKey;
    juce::String k = s.id + s.text + juce::String(loopA) + "/" + juce::String(loopB) + "/" + juce::String(spec.choruses) + (spec.countIn ? "c" : "") + juce::String(s.beatsPerBar);
    if (k != cachedKey) { cached = buildSequence(s.bars(), spec); cachedKey = k; }
    int countIn = cached.countInBeats;
    if (beat < countIn) { chart.setPosition(-1, -1, -1); setStatus("Conto: " + juce::String(countIn - (int)beat)); return; }
    const BeatSlot* slot = nullptr; int chorus = 0;
    if (cached.slotAt(beat, slot, chorus)) {
        chart.setPosition(slot->barIndex, slot->chordIndex, slot->beatInBar);
        int n = spec.choruses;
        setStatus("giro " + juce::String(chorus) + (n > 0 ? "/" + juce::String(n) : "") + juce::String(juce::CharPointer_UTF8("  \u00B7  ")) + juce::String(slot->barIndex + 1) + "." + juce::String(slot->beatInBar + 1));
        auto r = chart.getLocalBounds();
        int rowY = (slot->barIndex / 4) * 86;
        if (rowY < chartView.getViewPositionY() || rowY + 100 > chartView.getViewPositionY() + chartView.getViewHeight()) chartView.setViewPosition(0, juce::jmax(0, rowY - 10));
        juce::ignoreUnused(r);
    }
}
