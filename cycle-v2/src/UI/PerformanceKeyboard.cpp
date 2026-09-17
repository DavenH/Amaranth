#include "UI/PerformanceKeyboard.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
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
    setVelocity(1.f, true);
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
    setRangeStart(rangeStart + octaveDelta * 12);
}

void PerformanceKeyboard::revealNote(int midiNote) {
    const int selectedNote = jlimit(0, 127, midiNote);
    int nextStart = rangeStart;
    while (selectedNote < nextStart) {
        nextStart = jmax(0, nextStart - 12);
    }
    while (selectedNote > nextStart + visibleSemitones) {
        nextStart = jmin(127 - visibleSemitones, nextStart + 12);
    }
    setRangeStart(nextStart);
}

void PerformanceKeyboard::setRangeStart(int noteNumber) {
    const int nextStart = jlimit(0, 127 - visibleSemitones, noteNumber);
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
    constexpr int whiteKeyCount = 15;
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

bool PerformanceKeyboard::mouseDownOnKey(
        int midiNoteNumber,
        const MouseEvent& event) {
    if (event.mods.isRightButtonDown() || event.mods.isPopupMenu()) {
        if (previewNoteSelected) {
            previewNoteSelected(midiNoteNumber);
        }
        return false;
    }
    if (primaryGestureStarted) {
        primaryGestureStarted();
    }
    return true;
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
        keyboardState(state)
    ,   eventSink(sink)
    ,   keyboard(state, sink) {
    setName("PerformanceKeyboardPanel");
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(keyboard);
    addAndMakeVisible(octaveDown);
    addAndMakeVisible(octaveUp);
    addAndMakeVisible(modWheel);
    addAndMakeVisible(playButton);

    octaveDown.setTooltip("Lower keyboard by one octave");
    octaveUp.setTooltip("Raise keyboard by one octave");
    modWheel.setTooltip("Preview modulation wheel");
    playButton.setTooltip("Play the preview note for the voice duration");
    octaveDown.onClick = [this] {
        stopPlayback();
        keyboard.shiftOctave(-1);
    };
    octaveUp.onClick = [this] {
        stopPlayback();
        keyboard.shiftOctave(1);
    };
    playButton.onClick = [this] { togglePlayback(); };
    modWheel.onValueChanged = [this](int value) {
        sendModWheelValue();
        if (modWheelValueChanged) {
            modWheelValueChanged(value);
        }
    };
    modWheel.onGestureStarted = [this] {
        if (modWheelGestureStarted) {
            modWheelGestureStarted();
        }
    };
    modWheel.onGestureEnded = [this] {
        if (modWheelGestureEnded) {
            modWheelGestureEnded();
        }
    };
    keyboard.setPrimaryGestureStartedCallback([this] { stopPlayback(); });
    keyboard.setHighlightedNote(selectedPreviewNote);
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

Rectangle<float> PerformanceKeyboardPanel::modWheelBounds() const {
    return modWheel.getBounds().toFloat();
}

Rectangle<float> PerformanceKeyboardPanel::playButtonBounds() const {
    return playButton.getBounds().toFloat();
}

Rectangle<float> PerformanceKeyboardPanel::progressBounds() const {
    const Rectangle<float> button = playButtonBounds();
    return {
            0.f,
            button.getY(),
            (float) getWidth(),
            button.getHeight()
    };
}

void PerformanceKeyboardPanel::setPreviewNote(int midiNote) {
    selectedPreviewNote = jlimit(0, 127, midiNote);
    keyboard.revealNote(selectedPreviewNote);
    keyboard.setHighlightedNote(selectedPreviewNote);
}

void PerformanceKeyboardPanel::setPreviewNoteSelectedCallback(
        std::function<void(int)> callback) {
    keyboard.setPreviewNoteSelectedCallback([this, callback = std::move(callback)](int note) {
        setPreviewNote(note);
        if (callback) {
            callback(note);
        }
    });
}

void PerformanceKeyboardPanel::setModWheelValueChangedCallback(
        std::function<void(int)> callback) {
    modWheelValueChanged = std::move(callback);
}

void PerformanceKeyboardPanel::setModWheelGestureStartedCallback(
        std::function<void()> callback) {
    modWheelGestureStarted = std::move(callback);
}

void PerformanceKeyboardPanel::setModWheelGestureEndedCallback(
        std::function<void()> callback) {
    modWheelGestureEnded = std::move(callback);
}

void PerformanceKeyboardPanel::setModWheelValue(int value) {
    modWheel.setValue(value, true);
}

void PerformanceKeyboardPanel::setPlaybackDurationSeconds(float seconds) {
    playbackDuration = jmax(0.001f, seconds);
}

bool PerformanceKeyboardPanel::startPlayback(double nowMilliseconds) {
    stopPlayback();
    playbackNote = selectedPreviewNote;
    progress = 0.f;
    playbackStartedAtMilliseconds = nowMilliseconds;
    playing = true;
    sendModWheelValue();
    keyboardState.noteOn(1, playbackNote, 0.8f);
    startTimerHz(60);
    repaint();
    return true;
}

void PerformanceKeyboardPanel::togglePlayback() {
    if (playing) {
        stopPlayback();
        return;
    }
    startPlayback(Time::getMillisecondCounterHiRes());
}

void PerformanceKeyboardPanel::stopPlayback(bool resetProgress) {
    stopTimer();
    if (playbackNote >= 0) {
        keyboardState.noteOff(1, playbackNote, 0.f);
    }
    playbackNote = -1;
    playing = false;
    if (resetProgress) {
        progress = 0.f;
    }
    repaint();
}

void PerformanceKeyboardPanel::updatePlayback(double nowMilliseconds) {
    if (!playing) {
        return;
    }
    const double elapsedSeconds = jmax(
            0.0,
            (nowMilliseconds - playbackStartedAtMilliseconds) / 1000.0);
    progress = jlimit(
            0.f,
            1.f,
            (float) (elapsedSeconds / (double) playbackDuration));
    if (progress >= 1.f) {
        stopPlayback(false);
    }
    repaint(progressBounds().getSmallestIntegerContainer().expanded(2));
}

void PerformanceKeyboardPanel::releaseAllNotes() {
    stopPlayback();
    keyboard.releaseAllNotes();
}

void PerformanceKeyboardPanel::paint(Graphics& graphics) {
    const Rectangle<float> bounds = getLocalBounds().toFloat().reduced(0.75f);
    CanvasUtilityDock::paintSurface(graphics, bounds);

    const Rectangle<float> track = progressBounds();
    graphics.setColour(CanvasChromePalette::raisedSurface.withAlpha(0.34f));
    graphics.fillRect(track);
    if (progress > 0.f) {
        graphics.setColour(CanvasChromePalette::focus.withAlpha(0.13f));
        graphics.fillRect(track.withWidth(track.getWidth() * progress));
    }
}

void PerformanceKeyboardPanel::resized() {
    constexpr int transportHeight = 31;
    const bool compact = getWidth() < roundToInt(CanvasUtilityDock::preferredKeyboardWidth);
    const int panelInset = compact ? 4 : 6;
    const int buttonWidth = compact ? 25 : 28;
    const int controlGap = compact ? 2 : 4;
    const int wheelGap = compact ? 3 : 6;
    const int wheelWidth = compact ? 24 : 32;
    Rectangle<int> content = getLocalBounds().reduced(panelInset);
    Rectangle<int> transport = content.removeFromTop(transportHeight);
    playButton.setBounds(transport.withSizeKeepingCentre(buttonWidth, transportHeight));
    content.removeFromTop(controlGap);
    modWheel.setBounds(content.removeFromLeft(wheelWidth));
    content.removeFromLeft(wheelGap);
    octaveDown.setBounds(content.removeFromLeft(buttonWidth));
    content.removeFromLeft(controlGap);
    octaveUp.setBounds(content.removeFromRight(buttonWidth));
    content.removeFromRight(controlGap);
    keyboard.setBounds(content);
}

PerformanceKeyboardPanel::ModWheel::ModWheel() {
    setName("PerformanceKeyboard.ModWheel");
    setMouseCursor(MouseCursor::UpDownResizeCursor);
    setWantsKeyboardFocus(true);
}

void PerformanceKeyboardPanel::ModWheel::setValue(
        int value,
        bool sendNotification) {
    const int nextValue = jlimit(0, 127, value);
    if (nextValue == currentValue) {
        return;
    }
    currentValue = nextValue;
    repaint();
    if (sendNotification && onValueChanged) {
        onValueChanged(currentValue);
    }
}

bool PerformanceKeyboardPanel::ModWheel::keyPressed(const KeyPress& key) {
    const int keyCode = key.getKeyCode();
    if (keyCode != KeyPress::upKey && keyCode != KeyPress::downKey) {
        return false;
    }
    setValue(currentValue + (keyCode == KeyPress::upKey ? 1 : -1), true);
    return true;
}

void PerformanceKeyboardPanel::ModWheel::focusGained(FocusChangeType) {
    repaint();
}

void PerformanceKeyboardPanel::ModWheel::focusLost(FocusChangeType) {
    repaint();
}

void PerformanceKeyboardPanel::ModWheel::mouseDown(const MouseEvent& event) {
    if (!event.mods.isLeftButtonDown()) {
        return;
    }
    if (onGestureStarted) {
        onGestureStarted();
    }
    if (isShowing()) {
        grabKeyboardFocus();
    }
    updateFromPointer(event.position.y);
}

void PerformanceKeyboardPanel::ModWheel::mouseDrag(const MouseEvent& event) {
    if (!event.mods.isLeftButtonDown()) {
        return;
    }
    updateFromPointer(event.position.y);
}

void PerformanceKeyboardPanel::ModWheel::mouseUp(const MouseEvent&) {
    if (onGestureEnded) {
        onGestureEnded();
    }
}

void PerformanceKeyboardPanel::ModWheel::paint(Graphics& graphics) {
    const Rectangle<float> track = wheelTrack();
    const float proportion = (float) currentValue / 127.f;
    const float thumbY = jmap(proportion, track.getBottom(), track.getY());
    const bool focused = hasKeyboardFocus(true);

    graphics.setColour(CanvasChromePalette::raisedSurface.withAlpha(0.86f));
    graphics.fillRoundedRectangle(track, track.getWidth() * 0.5f);
    graphics.setColour(CanvasChromePalette::focus.withAlpha(0.45f));
    graphics.fillRoundedRectangle(
            track.withTop(thumbY),
            track.getWidth() * 0.5f);

    Rectangle<float> thumb(0.f, 0.f, jmin(18.f, getWidth() - 4.f), 16.f);
    thumb.setCentre((float) getWidth() * 0.5f, thumbY);
    const auto colours = CanvasChromePalette::control(
            focused
                    ? CanvasChromeControlState::Focused
                    : CanvasChromeControlState::Resting);
    graphics.setColour(colours.surface);
    graphics.fillRoundedRectangle(thumb, CanvasChromeMetrics::controlCornerRadius);
    graphics.setColour(colours.border);
    graphics.drawRoundedRectangle(
            thumb,
            CanvasChromeMetrics::controlCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);
    graphics.setColour(colours.text.withAlpha(0.82f));
    graphics.drawHorizontalLine(
            roundToInt(thumb.getCentreY()),
            thumb.getX() + 4.f,
            thumb.getRight() - 4.f);

    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(9.f, Font::plain));
    graphics.drawText(
            "MOD",
            getLocalBounds().removeFromBottom(13),
            Justification::centred,
            false);
}

Rectangle<float> PerformanceKeyboardPanel::ModWheel::wheelTrack() const {
    Rectangle<float> track = getLocalBounds().toFloat();
    track.removeFromTop(9.f);
    track.removeFromBottom(18.f);
    return track.withSizeKeepingCentre(6.f, track.getHeight());
}

void PerformanceKeyboardPanel::ModWheel::updateFromPointer(float y) {
    const Rectangle<float> track = wheelTrack();
    if (track.isEmpty()) {
        return;
    }
    const float proportion = 1.f - jlimit(
            0.f,
            1.f,
            (y - track.getY()) / track.getHeight());
    setValue(roundToInt(proportion * 127.f), true);
}

PerformanceKeyboardPanel::PlayButton::PlayButton(
        const PerformanceKeyboardPanel& panel) :
        Button  ("Preview playback")
    ,   owner   (panel) {
    setName("PerformanceKeyboard.Play");
    setMouseCursor(MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true);
}

void PerformanceKeyboardPanel::PlayButton::paintButton(
        Graphics& graphics,
        bool highlighted,
        bool down) {
    Rectangle<float> bounds = getLocalBounds().toFloat().reduced(2.f);
    const auto colours = CanvasChromePalette::control(
            highlighted || hasKeyboardFocus(true)
                    ? CanvasChromeControlState::Focused
                    : CanvasChromeControlState::Resting);
    graphics.setColour(colours.surface.brighter(down ? 0.08f : 0.f));
    graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::controlCornerRadius);
    graphics.setColour(colours.border);
    graphics.drawRoundedRectangle(
            bounds,
            CanvasChromeMetrics::controlCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);

    const Rectangle<float> glyph = bounds.withSizeKeepingCentre(10.f, 10.f);
    graphics.setColour(colours.text);
    if (owner.isPlaying()) {
        graphics.fillRect(glyph.reduced(1.f));
        return;
    }
    Path play;
    play.startNewSubPath(glyph.getX() + 1.f, glyph.getY());
    play.lineTo(glyph.getRight(), glyph.getCentreY());
    play.lineTo(glyph.getX() + 1.f, glyph.getBottom());
    play.closeSubPath();
    graphics.fillPath(play);
}

void PerformanceKeyboardPanel::timerCallback() {
    updatePlayback(Time::getMillisecondCounterHiRes());
}

void PerformanceKeyboardPanel::sendModWheelValue() {
    eventSink.enqueueMidiMessage(
            MidiMessage::controllerEvent(1, 1, modWheel.value()),
            MidiEventSource::PerformanceKeyboard);
}

}
