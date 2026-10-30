#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"

class GiroApplication : public juce::JUCEApplication, private juce::MenuBarModel {
public:
    const juce::String getApplicationName() override { return "Giro"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override {
        window = std::make_unique<MainWindow>(getApplicationName());
#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(this);
#endif
    }
    void shutdown() override {
#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
        window = nullptr;
    }
    void systemRequestedQuit() override { quit(); }

    enum { cmdNew = 1, cmdImport, cmdPlay, cmdTrUp, cmdTrDown };
    juce::StringArray getMenuBarNames() override { return {"File", "Brano"}; }
    juce::PopupMenu getMenuForIndex(int index, const juce::String&) override {
        juce::PopupMenu m;
        if (index == 0) {
            m.addItem(cmdNew, "Nuovo brano", true, false); m.addItem(cmdImport, juce::CharPointer_UTF8("Importa MIDI…"), true, false);
        } else {
            m.addItem(cmdPlay, "Play / Stop"); m.addSeparator();
            m.addItem(cmdTrUp, "Trasponi su"); m.addItem(cmdTrDown, juce::CharPointer_UTF8("Trasponi giù"));
        }
        return m;
    }
    void menuItemSelected(int id, int) override {
        auto* mc = window ? dynamic_cast<MainComponent*>(window->getContentComponent()) : nullptr;
        if (!mc) return;
        switch (id) {
        case cmdNew: mc->newSong(); break;
        case cmdImport: mc->importMidi(); break;
        case cmdPlay: mc->togglePlay(); break;
        case cmdTrUp: mc->transpose(1); break;
        case cmdTrDown: mc->transpose(-1); break;
        default: break;
        }
    }

    class MainWindow : public juce::DocumentWindow {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, juce::Colour(0xff15171c), DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);
            setResizeLimits(900, 600, 4000, 3000);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION(GiroApplication)
