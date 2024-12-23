#include <string>
#include <Binary/Images.h>
#include <App/Settings.h>
#include <App/AppConstants.h>
#include <App/SingletonRepo.h>
#include <UI/IConsole.h>
#include <Definitions.h>

#include "MidiKeyboard.h"

#include "../../Audio/SampleUtils.h"
#include "../../Inter/EnvelopeInter2D.h"

MidiKeyboard::MidiKeyboard(
        SingletonRepo* repo
    ,	MidiKeyboardState& state
    ,	const Orientation orientation) :
            MidiKeyboardComponent(state, orientation)
        ,	SingletonAccessor(repo, "MidiKeyboard")
        ,	auditionKey(60) {
    glow.setGlowProperties(3, Colours::white);

    setKeyWidth(12);
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    setKeyPressBaseOctave(3);

    keys = PNGImageFormat::loadFrom(Images::keys2_png, Images::keys2_pngSize);
}

void MidiKeyboard::drawBlackNote(int midiNoteNumber, Graphics& g, int x, int y,
                                 int w, int h, bool isDown, bool isOver,
                                 const Colour& noteFillColour) {
    g.setOpacity(1.f);
    g.drawImage(keys, x, y, w, h, pxGreyBlack, keys.getHeight() - h, w, h, false);

    if(midiNoteNumber != auditionKey)
        g.setOpacity(jmin(1.f, getVelocityA() / 0.7f));

    if (isDown || isOver || shouldDrawAuditionKey(midiNoteNumber)) {
        int offset = isDown ? pxOrangeBlack : pxBlueBlack;
        g.drawImage(keys, x, y, w, h, offset, keys.getHeight() - h, w, h, false);
    }

    Range<int> range(Constants::LowestMidiNote, Constants::HighestMidiNote);

    if (midiRange.contains(midiNoteNumber) && midiRange != range) {
        g.setOpacity(0.3);
        g.drawImage(keys, x, y, w, h, pxOrangeBlack, keys.getHeight() - h, w, h, false);
    }

    g.setOpacity(1.f);

    if(isOver) {
        showMsg(getText(midiNoteNumber));
    }
}

void MidiKeyboard::drawWhiteNote(int midiNoteNumber, Graphics& g, int x, int y,
                                 int w, int h, bool isDown, bool isOver,
                                 const Colour& lineColourDefault,
                                 const Colour& textColour) {
    g.drawImage(keys, x, y, w, h, pxGreyWhite, keys.getHeight() - h, w, h, false);

    if(midiNoteNumber != auditionKey)
        g.setOpacity(getVelocityA());

    if (isDown || isOver || shouldDrawAuditionKey(midiNoteNumber)) {
        int offset = isDown ? pxOrangeWhite : pxBlueWhite;
        g.drawImage(keys, x, y, w, h, offset, keys.getHeight() - h, w, h, false);
    }

    Range<int> range(getConstant(LowestMidiNote), getConstant(HighestMidiNote));

    if (midiRange.contains(midiNoteNumber) && midiRange != range) {
        g.setOpacity(0.3);
        g.drawImage(keys, x, y, w, h, pxOrangeWhite, keys.getHeight() - h, w, h, false);
    }

    g.setOpacity(1.f);

    if(isOver) {
        showMsg(getText(midiNoteNumber));
    }
}

void MidiKeyboard::drawUpDownButton(Graphics& g, int w, int h,
                                    const bool isMouseOver,
                                    const bool isButtonPressed,
                                    const bool movesOctavesUp) {
    g.fillAll(Colour::greyLevel(0.12f));

    float angle = movesOctavesUp ? 0.0f : 0.5f;

    Path path;

    path.lineTo(0.0f, 1.0f);
    path.lineTo(1.0f, 0.5f);
    path.closeSubPath();
    path.applyTransform(AffineTransform::rotation(MathConstants<float>::pi * 2.0f * angle, 0.5f, 0.5f));

    g.setColour(Colour::greyLevel(0.5f).withAlpha(isButtonPressed ? 1.0f : (isMouseOver ? 0.6f : 0.25f)));
    g.fillPath(path, path.getTransformToScaleToFit(3.0f, 16.0f, w - 6.0f, h - 32.0f, false));
}

String MidiKeyboard::getText(int note) {
    note -= 12;

    if(note < 0) {
        return {};
    }

    int letterIdx = note % 12;

    jassert(letterIdx >= 0 && letterIdx < 12);

    String letter[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    return letter[letterIdx] << String(note / 12 - 1);
}

void MidiKeyboard::getKeyPosition(int midiNoteNumber, float keyWidth, int& x, int& w) const {
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

    x = int(octave * 7.0f * keyWidth + notePos [note] * keyWidth + offsets[note] + 0.5f);
    w = int(widths [note] * keyWidth + 0.5f);
}

void MidiKeyboard::mouseEnter(const MouseEvent& e) {
    getObj(IConsole).setMouseUsage(true, true, false, true);
    getObj(IConsole).setKeys({});

    MidiKeyboardComponent::mouseEnter(e);
}

void MidiKeyboard::mouseMove(const MouseEvent& e) {
    setVelocity(e.y / float(getHeight()), true);

    int mouseNote = getMouseNote();
    repaintNote (mouseNote);

    MidiKeyboardComponent::mouseMove(e);

    String message = getText(mouseNote);

    if(mouseNote == auditionKey) {
        message = message + " (audition key)";
    }

    showMsg(message);
}

void MidiKeyboard::mouseExit(const MouseEvent& e) {
    setVelocity(1.f, false);

    MidiKeyboardComponent::mouseExit(e);
}

bool MidiKeyboard::mouseDownOnKey(int midiNoteNumber, const MouseEvent& e) {
    if (e.mods.isRightButtonDown()) {
        setAuditionKey(midiNoteNumber);

        getObj(SampleUtils).updateMidiNoteNumber(midiNoteNumber);
        return false;
    }

    setVelocity(e.y / float(getHeight()), true);
    int mouseNote = getMouseNote();

    repaintNote (mouseNote);
    setMouseNote(-1);

    updateNote(e.getPosition().toFloat());
    return false;
}

bool MidiKeyboard::mouseDraggedToKey(int midiNoteNumber, const MouseEvent& e) {
    setVelocity(e.y / float(getHeight()), true);
    updateNote(e.getPosition().toFloat());
    //Return true if you want the drag to trigger the new note, or false if you want to handle it yourself
    return false;
}

void MidiKeyboard::setAuditionKey(int key) {
    int oldAuditionKey = auditionKey;
    auditionKey = key;

    repaintNote (auditionKey);

    if(oldAuditionKey != auditionKey) {
        repaintNote (oldAuditionKey);
    }
}

void MidiKeyboard::resized() {
    MidiKeyboardComponent::resized();
}

void MidiKeyboard::updateNote(const Point<float>& point) {
    updateNoteUnderMouse(point, true, 0);
}

float MidiKeyboard::getVelocityA() {
    return 0.8; // don't know where `getVelocity()` went - must have come from midi keyboard? Did I edit that file?
}

bool MidiKeyboard::shouldDrawAuditionKey(int note) const {
    bool drawAuditionKey = note == auditionKey;

    return drawAuditionKey;
}

int MidiKeyboard::getMouseNote() const {
    if (mouseOverNotes.size() == 0) {
        return -1;
    }

    return mouseOverNotes.getFirst();
}

void MidiKeyboard::setMouseNote(int note) {
}

void MidiKeyboard::setMidiRange(const Range<int>& range) {
    midiRange = range;
    repaint();
}
