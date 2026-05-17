#include <string>
#include <App/Settings.h>
#include <App/AppConstants.h>
#include <App/SingletonRepo.h>
#include <UI/IConsole.h>
#include <Definitions.h>

#include "MidiKeyboard.h"

#include "../../Audio/SampleUtils.h"
#include "../../Inter/EnvelopeInter2D.h"
#include "../../UI/Panels/Console.h"

MidiKeyboard::MidiKeyboard(
        SingletonRepo* repo
    ,	MidiKeyboardState& state
    ,	const Orientation orientation) :
            AmaranthMidiKeyboard(state, orientation)
        ,	SingletonAccessor(repo, "MidiKeyboard")
        ,	auditionKey(60) {
    glow.setGlowProperties(3, Colours::white);
    setHighlightedNote(auditionKey);

    setKeyWidth(12);
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    setKeyPressBaseOctave(3);
}

String MidiKeyboard::getText(int note) {
    return AmaranthMidiKeyboard::getText(note);
}

void MidiKeyboard::mouseEnter(const MouseEvent& e) {
    getObj(Console).setMouseUsage(true, true, false, true);
    getObj(Console).setKeys({});

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

    showConsoleMsg(message);
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
    setHighlightedNote(auditionKey);

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

int MidiKeyboard::getMouseNote() const {
    if (mouseOverNotes.size() == 0) {
        return -1;
    }

    return mouseOverNotes.getFirst();
}

void MidiKeyboard::setMouseNote(int note) {
}
