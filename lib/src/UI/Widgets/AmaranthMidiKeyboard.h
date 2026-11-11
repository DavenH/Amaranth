#pragma once

#include "JuceHeader.h"

using namespace juce;

class AmaranthMidiKeyboard :
        public MidiKeyboardComponent {
public:
    struct VectorKeyPalette {
        Colour naturalTop { 0xffb8b8b8 };
        Colour naturalBottom { 0xff484848 };
        Colour accidentalTop { 0xff707070 };
        Colour accidentalBottom { 0xff202020 };
        Colour pressedTop { 0xffb7642f };
        Colour pressedBottom { 0xff4f210d };
        Colour highlightedTop { 0xff4f89aa };
        Colour highlightedBottom { 0xff18384a };
    };

    AmaranthMidiKeyboard(MidiKeyboardState& state, Orientation orientation);
    void setVectorKeyPalette(const VectorKeyPalette& palette);

    static String getText(int note);

    void setHighlightedNote(int note);
    [[nodiscard]] int getHighlightedNote() const { return highlightedNote; }
    void setMidiRange(const Range<int>& range);
    void setUseVectorKeys(bool shouldUseVectorKeys);

protected:
    void drawBlackNote(int midiNoteNumber, Graphics& g, Rectangle<float> area,
                       bool isDown, bool isOver, Colour noteFillColour) override;

    void drawWhiteNote(int midiNoteNumber, Graphics& g, Rectangle<float> area,
                       bool isDown, bool isOver, Colour lineColourDefault,
                       Colour textColour) override;

    void drawUpDownButton(Graphics& g, int w, int h,
                          bool isMouseOver,
                          bool isButtonPressed,
                          bool movesOctavesUp) override;

    Range<float> getKeyPosition(int midiNoteNumber, float keyWidth) const override;

private:
    enum {
        pxGreyBlack     = 0,
        pxGreyWhite     = 8,
        pxBlueBlack     = 20,
        pxBlueWhite     = 28,
        pxOrangeBlack   = 40,
        pxOrangeWhite   = 48,
    };

    void drawKeyImage(Graphics& g, Rectangle<float> area, int sourceY) const;
    void drawVectorKey(Graphics& g, Rectangle<float> area, bool blackKey,
        bool isDown, bool isOver, bool isHighlighted) const;

    Image keys;
    Range<int> midiRange;
    int highlightedNote = -1;
    bool useVectorKeys = false;
    VectorKeyPalette vectorKeyPalette;

    JUCE_LEAK_DETECTOR(AmaranthMidiKeyboard)
};
