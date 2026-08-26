#include "UI/PerformanceKeyboard.h"

#include "UI/CanvasUtilityDock.h"

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

String PerformanceKeyboard::noteLabel(int noteNumber) const {
    return noteNumber % 12 == 0
            ? AmaranthMidiKeyboard::getText(noteNumber)
            : String {};
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

void PerformanceKeyboard::drawWhiteNote(
        int midiNoteNumber,
        Graphics& graphics,
        Rectangle<float> area,
        bool isDown,
        bool isOver,
        Colour lineColour,
        Colour textColour) {
    AmaranthMidiKeyboard::drawWhiteNote(
            midiNoteNumber,
            graphics,
            area,
            isDown,
            isOver,
            lineColour,
            textColour);

    const String label = noteLabel(midiNoteNumber);
    if (label.isEmpty()) {
        return;
    }
    const float labelHeight = jlimit(10.f, 14.f, area.getHeight() * 0.2f);
    const Rectangle<float> labelBounds = area
            .removeFromBottom(labelHeight + 3.f)
            .reduced(2.f, 0.f);
    graphics.setColour(Colours::white.withAlpha(isDown ? 0.92f : 0.72f));
    graphics.setFont(FontOptions(labelHeight * 0.72f, Font::plain));
    graphics.drawText(label, labelBounds, Justification::centred, false);
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

    octaveDown.setTooltip("Lower keyboard by one octave");
    octaveUp.setTooltip("Raise keyboard by one octave");
    octaveDown.onClick = [this] { keyboard.shiftOctave(-1); };
    octaveUp.onClick = [this] { keyboard.shiftOctave(1); };
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

void PerformanceKeyboardPanel::paint(Graphics& graphics) {
    const Rectangle<float> bounds = getLocalBounds().toFloat().reduced(0.75f);
    CanvasUtilityDock::paintSurface(graphics, bounds);
    graphics.drawHorizontalLine(headerHeight(), 6.f, (float) getWidth() - 6.f);
    graphics.setColour(Colour(0xff8793a1));
    graphics.setFont(FontOptions(10.f, Font::bold));
    Rectangle<int> title = headerBounds();
    const int buttonWidth = jmin(28, title.getHeight() + 4);
    title.removeFromLeft(buttonWidth);
    title.removeFromRight(buttonWidth);
    graphics.drawText("Keyboard", title, Justification::centred);
}

void PerformanceKeyboardPanel::resized() {
    Rectangle<int> content = getLocalBounds().reduced(6);
    Rectangle<int> header = headerBounds();
    content.removeFromTop(headerHeight() - 6);
    const int buttonWidth = jmin(28, header.getHeight() + 4);
    octaveDown.setBounds(header.removeFromLeft(buttonWidth).reduced(1));
    octaveUp.setBounds(header.removeFromRight(buttonWidth).reduced(1));
    content.removeFromTop(4);
    keyboard.setBounds(content);
}

Rectangle<int> PerformanceKeyboardPanel::headerBounds() const {
    Rectangle<int> content = getLocalBounds().reduced(6);
    return content.removeFromTop(headerHeight() - 6);
}

int PerformanceKeyboardPanel::headerHeight() const {
    return jlimit(20, 30, getHeight() / 4);
}

}
