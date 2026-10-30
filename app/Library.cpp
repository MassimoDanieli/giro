#include "Library.h"
#include "../core/theory.h"

using namespace giro;

void Song::transpose(int semis) {
    if (semis == 0) return;
    std::string newKey = transposeKey(key.toStdString(), semis);
    bool flats = keyPrefersFlats(newKey);
    auto bars = transposeBars(parseChart(text.toStdString(), beatsPerBar), semis, flats);
    text = juce::String(chartToText(bars, beatsPerBar));
    key = juce::String(newKey);
}

juce::var Song::toVar() const {
    auto* o = new juce::DynamicObject();
    o->setProperty("id", id); o->setProperty("title", title); o->setProperty("key", key);
    o->setProperty("tempo", tempo); o->setProperty("style", juce::String(styleName(style)));
    o->setProperty("beatsPerBar", beatsPerBar); o->setProperty("text", text);
    return juce::var(o);
}

Song Song::fromVar(const juce::var& v) {
    Song s;
    s.id = v.getProperty("id", newId()).toString();
    s.title = v.getProperty("title", "").toString();
    s.key = v.getProperty("key", "C").toString();
    s.tempo = (double)v.getProperty("tempo", 120.0);
    s.style = styleFromName(v.getProperty("style", "swing").toString().toStdString());
    s.beatsPerBar = juce::jlimit(1, 12, (int)v.getProperty("beatsPerBar", 4));
    s.text = v.getProperty("text", "").toString();
    if (s.id.isEmpty()) s.id = newId();
    return s;
}

juce::String Song::newId() { return juce::Uuid().toString(); }

static Song mk(const char* title, const char* key, double tempo, Style style, int bpb, const char* text) {
    Song s; s.id = Song::newId(); s.title = juce::CharPointer_UTF8(title); s.key = key; s.tempo = tempo; s.style = style; s.beatsPerBar = bpb; s.text = text; return s;
}

std::vector<Song> Library::samples() {
    return {
        mk("Blues in F", "F", 132, Style::Swing, 4, "[A]\n| F7 | Bb7 | F7 | Cm7 F7 |\n| Bb7 | Bdim7 | F7 | Am7b5 D7 |\n| Gm7 | C7 | F7 D7 | Gm7 C7 |"),
        mk("Giro di Do", "C", 110, Style::Straight, 4, "[A]\n| C | Am7 | Dm7 | G7 |\n| C | Am7 | Dm7 | G7 |\n[B]\n| Fmaj7 | Em7 | Dm7 | G7 |\n| C | Am7 | Dm7 G7 | C |"),
        mk("Bossa in Re minore", "Dm", 126, Style::Bossa, 4, "[A]\n| Dm7 | Dm7 | Em7b5 | A7b9 |\n| Dm7 | Gm7 C7 | Fmaj7 | Em7b5 A7 |\n[B]\n| Dm7 | Bbmaj7 | Em7b5 | A7b9 |\n| Dm7 | Gm7 C7 | Fmaj7 Bb7 | Dm7 A7 |"),
        mk("ii–V–I per tutte le tonalità", "C", 100, Style::Swing, 4, "| Dm7 G7 | Cmaj7 | Cm7 F7 | Bbmaj7 |\n| Bbm7 Eb7 | Abmaj7 | Abm7 Db7 | Gbmaj7 |\n| F#m7 B7 | Emaj7 | Em7 A7 | Dmaj7 |"),
        mk("Ballad in Mib", "Eb", 66, Style::Ballad, 4, "[A]\n| Ebmaj7 | Cm7 | Fm7 | Bb7 |\n| Gm7 | C7 | Fm7 | Bb7 |\n[B]\n| Ebmaj7 Eb7 | Abmaj7 Abm6 | Gm7 C7 | Fm7 Bb7 |\n| Ebmaj7 | Cm7 F7 | Fm7 Bb7 | Eb6 _ _ Bb7 |"),
        mk("Valzer in Sol", "G", 150, Style::Swing, 3, "| G | Em7 | Am7 | D7 |\n| Bm7 | E7 | Am7 | D7 |\n| Gmaj7 | G7 | Cmaj7 | Cm6 |\n| Bm7 | E7 | Am7 D7 | G |")
    };
}

const juce::StringArray& Library::keys() {
    static const juce::StringArray K = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B", "Cm", "C#m", "Dm", "Ebm", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "Bbm", "Bm"};
    return K;
}

Library::Library() { load(); }

juce::File Library::file() {
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Giro");
    dir.createDirectory();
    return dir.getChildFile("library.json");
}

void Library::load() {
    songs.clear();
    auto v = juce::JSON::parse(file());
    if (auto* arr = v.getProperty("songs", juce::var()).getArray())
        for (auto& s : *arr) songs.push_back(Song::fromVar(s));
    selectedId = v.getProperty("selected", "").toString();
    if (songs.empty()) songs = samples();
    if (selectedIndex() < 0) selectedId = songs[0].id;
}

void Library::save() const {
    juce::Array<juce::var> arr;
    for (auto& s : songs) arr.add(s.toVar());
    auto* o = new juce::DynamicObject();
    o->setProperty("songs", arr); o->setProperty("selected", selectedId);
    file().replaceWithText(juce::JSON::toString(juce::var(o)));
}

int Library::selectedIndex() const {
    for (int i = 0; i < (int)songs.size(); i++) if (songs[i].id == selectedId) return i;
    return -1;
}

Song& Library::selected() {
    int i = selectedIndex();
    if (i < 0) { if (songs.empty()) songs = samples(); selectedId = songs[0].id; i = 0; }
    return songs[(size_t)i];
}

void Library::add(const Song& s) { songs.push_back(s); selectedId = s.id; save(); }

void Library::removeSelected() {
    int i = selectedIndex();
    if (i < 0) return;
    songs.erase(songs.begin() + i);
    if (songs.empty()) songs = samples();
    selectedId = songs[(size_t)juce::jmin(i, (int)songs.size() - 1)].id;
    save();
}
