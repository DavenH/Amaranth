#pragma once

#include "JuceHeader.h"
#include <App/SingletonAccessor.h>
#include <UI/Widgets/AmaranthMidiKeyboard.h>

using namespace juce;
class SingletonRepo;

class MidiKeyboard :
        public AmaranthMidiKeyboard
    ,	public SingletonAccessor {
public:
    friend class MidiKeyboardComponent;

    MidiKeyboard(SingletonRepo* repo, MidiKeyboardState& state, Orientation orientation);

    void mouseExit(const MouseEvent& e) override;
    void mouseEnter(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;
    static String getText(int note);

    [[nodiscard]] int getAuditionKey() const { return auditionKey; }
    void resized() override;
    void setAuditionKey(int key);

protected:
    bool mouseDownOnKey(int midiNoteNumber, const MouseEvent& e) override;
    bool mouseDraggedToKey(int midiNoteNumber, const MouseEvent& e) override;
    void updateNote(const Point<float>&);

private:
    int getMouseNote() const;

    static void setMouseNote(int note);

    GlowEffect glow;
    int auditionKey;

    JUCE_LEAK_DETECTOR(MidiKeyboard)
};
