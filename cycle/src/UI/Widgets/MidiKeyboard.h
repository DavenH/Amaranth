#pragma once

#include "JuceHeader.h"
#include <App/SingletonAccessor.h>

using namespace juce;
class SingletonRepo;

class MidiKeyboard :
        public MidiKeyboardComponent
    ,	public SingletonAccessor {
public:
    friend class MidiKeyboardComponent;

    MidiKeyboard(SingletonRepo* repo, MidiKeyboardState& state, Orientation orientation);
    ~MidiKeyboard() override;

    void mouseExit(const MouseEvent& e) override;
    void mouseEnter(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;
    static String getText(int note);

    static float getVelocityA();
    [[nodiscard]] int getAuditionKey() const { return auditionKey; }
    void resized() override;
    void setAuditionKey(int key);
    void setMidiRange(const Range<int>& range);

protected:
    void drawBlackNote(int midiNoteNumber, Graphics &g, int x, int y, int w, int h, bool isDown,
            bool isOver, const Colour& noteFillColour);

    void drawWhiteNote(int midiNoteNumber, Graphics &g, int x, int y, int w, int h, bool isDown,
            bool isOver, const Colour& lineColourDefault,
            const Colour &textColour);

    void drawUpDownButton(Graphics& g, int w, int h, bool isMouseOver, bool isButtonPressed,
            bool movesOctavesUp) override;

    void getKeyPosition(int midiNoteNumber, float keyWidth, int& x, int& w) const;
    bool mouseDownOnKey(int midiNoteNumber, const MouseEvent& e) override;
    bool mouseDraggedToKey(int midiNoteNumber, const MouseEvent& e) override;
    void updateNote(const Point<float>&);

private:
    enum {
        pxGreyBlack 	= 0,
        pxGreyWhite 	= 8,
        pxBlueBlack 	= 20,
        pxBlueWhite 	= 28,
        pxOrangeBlack 	= 40,
        pxOrangeWhite 	= 48,
    };

    int getMouseNote() const;

    static void setMouseNote(int note);
    bool shouldDrawAuditionKey(int note) const;

    GlowEffect glow;
    Image keys;

    Range<int> midiRange;
    int auditionKey;

    JUCE_LEAK_DETECTOR(MidiKeyboard)
};
