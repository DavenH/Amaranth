#pragma once

#include "JuceHeader.h"

using namespace juce;

class AmaranthMidiKeyboard :
        public MidiKeyboardComponent {
public:
    AmaranthMidiKeyboard(MidiKeyboardState& state, Orientation orientation);

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

    JUCE_LEAK_DETECTOR(AmaranthMidiKeyboard)
};
