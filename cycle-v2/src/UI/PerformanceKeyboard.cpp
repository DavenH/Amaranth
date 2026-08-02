#include "PerformanceKeyboard.h"

namespace CycleV2 {

PerformanceKeyboard::PerformanceKeyboard(
        MidiKeyboardState& state,
        MidiEventSink& sink) :
        AmaranthMidiKeyboard(state, MidiKeyboardComponent::horizontalKeyboard)
    ,   keyboardState(state)
    ,   eventSink(sink)
    ,   stateListener(*this) {
    setName("PerformanceKeyboard");
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    setScrollButtonsVisible(false);
    setUseVectorKeys(true);
    setMidiChannel(1);
    setVelocity(0.8f, true);
    setAvailableRange(rangeStart, rangeStart + visibleSemitones);
    setLowestVisibleKey(rangeStart);
    keyboardState.addListener(&stateListener);
}

PerformanceKeyboard::~PerformanceKeyboard() {
    releaseAllNotes();
    keyboardState.removeListener(&stateListener);
}

Rectangle<float> PerformanceKeyboard::noteBounds(int noteNumber) const {
    if (noteNumber < rangeStart || noteNumber > rangeStart + visibleSemitones) {
        return {};
    }
    return getRectangleForKey(noteNumber).getIntersection(getLocalBounds().toFloat());
}

void PerformanceKeyboard::shiftOctave(int octaveDelta) {
    const int nextStart = jlimit(0, 115, rangeStart + octaveDelta * 12);
    if (nextStart == rangeStart) {
        return;
    }

    releaseAllNotes();
    rangeStart = nextStart;
    setAvailableRange(rangeStart, rangeStart + visibleSemitones);
    setLowestVisibleKey(rangeStart);
    resized();
    repaint();
}

void PerformanceKeyboard::releaseAllNotes() {
    keyboardState.allNotesOff(1);
    eventSink.releaseMidiSource(MidiEventSource::PerformanceKeyboard);
    currentHeldNote = -1;
    currentVelocity = 0.f;
}

void PerformanceKeyboard::resized() {
    constexpr int whiteKeyCount = 8;
    if (getWidth() <= 0) {
        return;
    }
    setKeyWidth((float) getWidth() / (float) whiteKeyCount);
    AmaranthMidiKeyboard::resized();
}

void PerformanceKeyboard::StateListener::handleNoteOn(
        MidiKeyboardState*,
        int midiChannel,
        int midiNoteNumber,
        float velocity) {
    owner.handleNoteOn(midiChannel, midiNoteNumber, velocity);
}

void PerformanceKeyboard::StateListener::handleNoteOff(
        MidiKeyboardState*,
        int midiChannel,
        int midiNoteNumber,
        float velocity) {
    owner.handleNoteOff(midiChannel, midiNoteNumber, velocity);
}

void PerformanceKeyboard::handleNoteOn(
        int midiChannel,
        int midiNoteNumber,
        float velocity) {
    currentHeldNote = midiNoteNumber;
    currentVelocity = velocity;
    eventSink.enqueueMidiMessage(
            MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity),
            MidiEventSource::PerformanceKeyboard);
}

void PerformanceKeyboard::handleNoteOff(
        int midiChannel,
        int midiNoteNumber,
        float velocity) {
    eventSink.enqueueMidiMessage(
            MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity),
            MidiEventSource::PerformanceKeyboard);
    if (currentHeldNote == midiNoteNumber) {
        currentHeldNote = -1;
        currentVelocity = 0.f;
    }
}

PerformanceKeyboardPanel::PerformanceKeyboardPanel(
        MidiKeyboardState& state,
        MidiEventSink& sink) :
        keyboard(state, sink) {
    setName("PerformanceKeyboardPanel");
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(keyboard);
    addAndMakeVisible(octaveDown);
    addAndMakeVisible(octaveUp);
    addAndMakeVisible(audioStatus);

    octaveDown.setTooltip("Lower keyboard by one octave");
    octaveUp.setTooltip("Raise keyboard by one octave");
    octaveDown.onClick = [this] { keyboard.shiftOctave(-1); };
    octaveUp.onClick = [this] { keyboard.shiftOctave(1); };
    audioStatus.setJustificationType(Justification::centred);
    audioStatus.setInterceptsMouseClicks(false, false);
}

Rectangle<float> PerformanceKeyboardPanel::noteBounds(int noteNumber) const {
    return keyboard.noteBounds(noteNumber).translated(
            (float) keyboard.getX(),
            (float) keyboard.getY());
}

Rectangle<float> PerformanceKeyboardPanel::octaveDownBounds() const {
    return octaveDown.getBounds().toFloat();
}

Rectangle<float> PerformanceKeyboardPanel::octaveUpBounds() const {
    return octaveUp.getBounds().toFloat();
}

Rectangle<float> PerformanceKeyboardPanel::dragHandleBounds() const {
    return audioStatus.getBounds().toFloat();
}

void PerformanceKeyboardPanel::setStatus(const String& status) {
    if (audioStatus.getText() != status) {
        audioStatus.setText(status, dontSendNotification);
    }
}

void PerformanceKeyboardPanel::paint(Graphics& graphics) {
    const Rectangle<float> bounds = getLocalBounds().toFloat().reduced(0.75f);
    graphics.setColour(Colour(0xff171d24));
    graphics.fillRoundedRectangle(bounds, 7.f);
    graphics.setColour(Colour(0xff3d4a58));
    graphics.drawRoundedRectangle(bounds, 7.f, 1.5f);
    graphics.drawHorizontalLine(headerHeight(), 6.f, (float) getWidth() - 6.f);
}

void PerformanceKeyboardPanel::resized() {
    Rectangle<int> content = getLocalBounds().reduced(6);
    Rectangle<int> header = content.removeFromTop(headerHeight() - 6);
    const int buttonWidth = jmin(28, header.getHeight() + 4);
    octaveDown.setBounds(header.removeFromLeft(buttonWidth).reduced(1));
    octaveUp.setBounds(header.removeFromRight(buttonWidth).reduced(1));
    audioStatus.setBounds(header);
    content.removeFromTop(4);
    keyboard.setBounds(content);
}

void PerformanceKeyboardPanel::mouseDown(const MouseEvent& event) {
    dragGestureActive = event.position.y <= (float) headerHeight();
    if (!dragGestureActive) {
        return;
    }
    dragger.startDraggingComponent(this, event);
}

void PerformanceKeyboardPanel::mouseDrag(const MouseEvent& event) {
    if (!dragGestureActive) {
        return;
    }
    dragger.dragComponent(this, event, nullptr);
    if (onMoved) {
        onMoved(getPosition());
    }
}

void PerformanceKeyboardPanel::mouseUp(const MouseEvent&) {
    dragGestureActive = false;
}

int PerformanceKeyboardPanel::headerHeight() const {
    return jlimit(20, 30, getHeight() / 4);
}

}
