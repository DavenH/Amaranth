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
    if (useVectorKeys) {
        drawVectorKey(g, area, true, isDown, isOver, midiNoteNumber == highlightedNote);
        return;
    }

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
    if (useVectorKeys) {
        drawVectorKey(g, area, false, isDown, isOver, midiNoteNumber == highlightedNote);
        return;
    }

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

    const float pixelOffset = useVectorKeys ? 0.0f : (float) offsets[note] + 0.5f;
    const float widthAdjustment = useVectorKeys ? 0.0f : 0.5f;
    const float x = octave * 7.0f * keyWidth + notePos[note] * keyWidth + pixelOffset;
    const float w = widths[note] * keyWidth + widthAdjustment;

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

void AmaranthMidiKeyboard::setUseVectorKeys(bool shouldUseVectorKeys) {
    if (useVectorKeys != shouldUseVectorKeys) {
        useVectorKeys = shouldUseVectorKeys;
        resized();
        repaint();
    }
}

void AmaranthMidiKeyboard::drawVectorKey(
        Graphics& g, Rectangle<float> area, bool blackKey,
        bool isDown, bool isOver, bool isHighlighted) const {
    const bool accented = isDown || isOver || isHighlighted;
    const Colour top = accented
        ? (isDown ? Colour(0xffb7642f) : Colour(0xff4f89aa))
        : (blackKey ? Colour(0xff707070) : Colour(0xffb8b8b8));
    const Colour bottom = accented
        ? (isDown ? Colour(0xff4f210d) : Colour(0xff18384a))
        : (blackKey ? Colour(0xff202020) : Colour(0xff484848));

    const float corner = blackKey ? jmin(2.5f, area.getWidth() * 0.18f) : 1.0f;
    Path key;
    key.addRoundedRectangle(area.reduced(0.5f), corner);

    ColourGradient fill(top, area.getCentreX(), area.getY(),
        bottom, area.getCentreX(), area.getBottom(), false);
    fill.addColour(0.78, bottom.interpolatedWith(Colours::black, 0.18f));
    g.setGradientFill(fill);
    g.fillPath(key);

    g.setColour(Colours::black.withAlpha(0.9f));
    g.strokePath(key, PathStrokeType(1.0f));

    const Rectangle<float> highlight = area.reduced(1.5f).removeFromLeft(
        jmax(1.0f, area.getWidth() * 0.12f));
    g.setColour(Colours::white.withAlpha(blackKey ? 0.18f : 0.10f));
    g.fillRoundedRectangle(highlight, corner * 0.5f);
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
