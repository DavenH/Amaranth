#include "AmaranthMidiKeyboard.h"

#include <Binary/Images.h>

#include "../../App/AppConstants.h"

AmaranthMidiKeyboard::AmaranthMidiKeyboard(MidiKeyboardState& state, Orientation orientation) :
        MidiKeyboardComponent(state, orientation) {
    keys = PNGImageFormat::loadFrom(Images::keys2_png, Images::keys2_pngSize);
    jassert(! keys.isNull());
}

void AmaranthMidiKeyboard::drawBlackNote(int midiNoteNumber, Graphics& g, Rectangle<float> area,
                                         bool isDown, bool isOver, Colour noteFillColour) {
    drawKeyImage(g, area, pxGreyBlack);

    if (midiNoteNumber != highlightedNote) {
        g.setOpacity(1.0f);
    }

    if (isDown || isOver || midiNoteNumber == highlightedNote) {
        const int offset = isDown ? pxOrangeBlack : pxBlueBlack;
        drawKeyImage(g, area, offset);
    }

    const Range<int> fullRange(Constants::LowestMidiNote, Constants::HighestMidiNote);
    if (midiRange.contains(midiNoteNumber) && midiRange != fullRange) {
        g.setOpacity(0.3f);
        drawKeyImage(g, area, pxOrangeBlack);
    }

    g.setOpacity(1.0f);
}

void AmaranthMidiKeyboard::drawWhiteNote(int midiNoteNumber, Graphics& g, Rectangle<float> area,
                                         bool isDown, bool isOver, Colour lineColourDefault,
                                         Colour textColour) {
    drawKeyImage(g, area, pxGreyWhite);

    if (midiNoteNumber != highlightedNote) {
        g.setOpacity(0.8f);
    }

    if (isDown || isOver || midiNoteNumber == highlightedNote) {
        const int offset = isDown ? pxOrangeWhite : pxBlueWhite;
        drawKeyImage(g, area, offset);
    }

    const Range<int> fullRange(Constants::LowestMidiNote, Constants::HighestMidiNote);
    if (midiRange.contains(midiNoteNumber) && midiRange != fullRange) {
        g.setOpacity(0.3f);
        drawKeyImage(g, area, pxOrangeWhite);
    }

    g.setOpacity(1.0f);
}

void AmaranthMidiKeyboard::drawUpDownButton(Graphics& g, int w, int h,
                                            const bool isMouseOver,
                                            const bool isButtonPressed,
                                            const bool movesOctavesUp) {
    g.fillAll(Colour::greyLevel(0.12f));

    Path path;
    path.lineTo(0.0f, 1.0f);
    path.lineTo(1.0f, 0.5f);
    path.closeSubPath();

    if (! movesOctavesUp) {
        path.applyTransform(AffineTransform::rotation(MathConstants<float>::pi, 0.5f, 0.5f));
    }

    g.setColour(Colour::greyLevel(0.5f).withAlpha(isButtonPressed ? 1.0f : (isMouseOver ? 0.6f : 0.25f)));
    g.fillPath(path, path.getTransformToScaleToFit(3.0f, 16.0f, w - 6.0f, h - 32.0f, false));
}

String AmaranthMidiKeyboard::getText(int note) {
    note -= 12;

    if (note < 0) {
        return {};
    }

    const int letterIdx = note % 12;
    jassert(letterIdx >= 0 && letterIdx < 12);

    String letter[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    return letter[letterIdx] << String(note / 12 - 1);
}

Range<float> AmaranthMidiKeyboard::getKeyPosition(int midiNoteNumber, float keyWidth) const {
    jassert(midiNoteNumber >= 0 && midiNoteNumber < 128);

    static const float blackNoteWidth = 0.7f;

    static const float notePos[] = { 0.0f, 1 - blackNoteWidth * 0.6f,
                                     1.0f, 2 - blackNoteWidth * 0.4f,
                                     2.0f, 3.0f, 4 - blackNoteWidth * 0.7f,
                                     4.0f, 5 - blackNoteWidth * 0.5f,
                                     5.0f, 6 - blackNoteWidth * 0.3f,
                                     6.0f };

    static const float widths[] = { 1.0f, blackNoteWidth,
                                    1.0f, blackNoteWidth,
                                    1.0f, 1.0f, blackNoteWidth,
                                    1.0f, blackNoteWidth,
                                    1.0f, blackNoteWidth,
                                    1.0f };

    static const int offsets[] = { 0, 1, 0, 0, 0, 0, 2, 0, 1, 0, 1, 0 };
    const int octave = midiNoteNumber / 12;
    const int note = midiNoteNumber % 12;

    const float x = octave * 7.0f * keyWidth + notePos[note] * keyWidth + offsets[note] + 0.5f;
    const float w = widths[note] * keyWidth + 0.5f;

    return { x, x + w };
}

void AmaranthMidiKeyboard::setHighlightedNote(int note) {
    const int previousNote = highlightedNote;
    highlightedNote = note;

    repaintNote(highlightedNote);

    if (previousNote != highlightedNote) {
        repaintNote(previousNote);
    }
}

void AmaranthMidiKeyboard::setMidiRange(const Range<int>& range) {
    midiRange = range;
    repaint();
}

void AmaranthMidiKeyboard::drawKeyImage(Graphics& g, Rectangle<float> area, int sourceY) const {
    if (keys.isNull()) {
        return;
    }

    const int x = roundToInt(area.getX());
    const int y = roundToInt(area.getY());
    const int w = roundToInt(area.getWidth());
    const int h = roundToInt(area.getHeight());

    g.drawImage(keys, x, y, w, h, sourceY, keys.getHeight() - h, w, h, false);
}
