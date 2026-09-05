#include "UI/PerformanceKeyboard.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasUtilityDock.h"
#include "UI/WorkspaceDock.h"

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

PerformanceKeyboardPanel::OctaveButton::OctaveButton(bool advancesOctave) :
        Button      (advancesOctave ? "Next keyboard octave" : "Previous keyboard octave")
    ,   advances    (advancesOctave) {
    setName(advances ? "PerformanceKeyboard.OctaveUp" : "PerformanceKeyboard.OctaveDown");
    setMouseCursor(MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true);
}

void PerformanceKeyboardPanel::OctaveButton::paintButton(
        Graphics& graphics,
        bool highlighted,
        bool down) {
    const Rectangle<float> bounds = getLocalBounds().toFloat().reduced(0.5f);
    WorkspaceDock::paintIconButton(
            graphics,
            bounds,
            advances ? WorkspaceDockIcon::ChevronRight : WorkspaceDockIcon::ChevronLeft,
            highlighted || hasKeyboardFocus(true));
    if (down) {
        graphics.setColour(Colours::white.withAlpha(0.08f));
        graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::controlCornerRadius);
    }
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
}

void PerformanceKeyboardPanel::resized() {
    constexpr int buttonWidth = 28;
    constexpr int controlGap = 4;
    Rectangle<int> content = getLocalBounds().reduced(6);
    octaveDown.setBounds(content.removeFromLeft(buttonWidth));
    content.removeFromLeft(controlGap);
    octaveUp.setBounds(content.removeFromRight(buttonWidth));
    content.removeFromRight(controlGap);
    keyboard.setBounds(content);
}

}
