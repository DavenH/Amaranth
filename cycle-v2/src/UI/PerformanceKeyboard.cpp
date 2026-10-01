#include <algorithm>

#include "UI/PerformanceKeyboard.h"
#include "UI/PresetMidiEditor.h"

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

void PerformanceKeyboard::revealRange(int lowest, int highest) {
    int bestStart = rangeStart;
    int bestOverlap = -1;
    int bestOffset = 128;
    for (int candidate = 0; candidate <= 103; candidate = candidate == 96 ? 103 : candidate + 12) {
        const int overlap = jmax(0,
                jmin(highest, candidate + visibleSemitones)
                        - jmax(lowest, candidate) + 1);
        const int centresDelta = lowest + highest - 2 * (candidate + 12);
        const int offset = centresDelta < 0 ? -centresDelta : centresDelta;
        if (overlap > bestOverlap || (overlap == bestOverlap && offset < bestOffset)) {
            bestStart = candidate;
            bestOverlap = overlap;
            bestOffset = offset;
        }
    }
    setRangeStart(bestStart);
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
    addAndMakeVisible(editButton);
    addAndMakeVisible(recordButton);
    playButton.setName("PerformanceKeyboard.PlaySequence");
    editButton.setName("PerformanceKeyboard.EditSequence");
    recordButton.setName("PerformanceKeyboard.RecordSequence");
    playButton.setTooltip("Play or stop this preset preview phrase");
    editButton.setTooltip("Edit preset preview notes and modulation");
    recordButton.setTooltip("Record MIDI input into this preset preview");
    for (auto* button : { &playButton, &editButton, &recordButton }) {
        button->setColour(TextButton::buttonColourId,
                CanvasChromePalette::restingControlSurface);
        button->setColour(TextButton::buttonOnColourId,
                CanvasChromePalette::raisedSurface);
        button->setColour(TextButton::textColourOffId,
                CanvasChromePalette::text);
        button->setMouseCursor(MouseCursor::PointingHandCursor);
    }
    playButton.onClick = [this] { togglePlayback(); };
    editButton.onClick = [this] { openSequenceEditor(); };
    recordButton.onClick = [this] {
        if (recording) {
            stopRecording();
            return;
        }
        stopPlayback();
        if (!sequence.has_value()) {
            sequence = PresetMidiSequence {};
        }
        recordingStartedAtSeconds = Time::getMillisecondCounterHiRes() / 1000.0;
        recordedNoteStarts.fill(-1.0);
        recording = true;
        recordButton.setButtonText("STOP");
        if (recordingChanged) {
            recordingChanged(true);
        }
    };

    octaveDown.setTooltip("Lower keyboard by one octave");
    octaveUp.setTooltip("Raise keyboard by one octave");
    modWheel.setTooltip("Preview modulation wheel");
    octaveDown.onClick = [this] {
        stopPlayback();
        keyboard.shiftOctave(-1);
    };
    octaveUp.onClick = [this] {
        stopPlayback();
        keyboard.shiftOctave(1);
    };
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

Rectangle<float> PerformanceKeyboardPanel::progressBounds() const {
    const Rectangle<float> keys = keyboard.getBounds().toFloat();
    return {
            keys.getX(),
            (float) getHeight() - 4.f,
            keys.getWidth(),
            3.f
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

void PerformanceKeyboardPanel::setSequence(std::optional<PresetMidiSequence> nextSequence) {
    stopPlayback();
    stopRecording();
    releaseEditorAudition();
    sequence = std::move(nextSequence);
    rebuildPlaybackEvents();
    if (sequence.has_value() && !sequence->notes.empty()) {
        int lowest = 127;
        int highest = 0;
        for (const auto& note : sequence->notes) {
            lowest = jmin(lowest, note.pitch);
            highest = jmax(highest, note.pitch);
        }
        keyboard.revealRange(lowest, highest);
    }
}

void PerformanceKeyboardPanel::setSequenceChangedCallback(
        std::function<void(PresetMidiSequence)> callback) {
    sequenceChanged = std::move(callback);
}

bool PerformanceKeyboardPanel::hasSequenceNotes() const {
    return sequence.has_value() && !sequence->notes.empty();
}

void PerformanceKeyboardPanel::auditionSequenceNote(
        int pitch, int velocity, bool noteOn) {
    if (noteOn) {
        stopPlayback();
        releaseEditorAudition();
        editorAuditionPitch = pitch;
        setPreviewNote(pitch);
        keyboardState.noteOn(1, pitch, (float) velocity / 127.f);
    } else if (editorAuditionPitch == pitch) {
        releaseEditorAudition();
    }
}

void PerformanceKeyboardPanel::releaseEditorAudition() {
    if (editorAuditionPitch >= 0) {
        keyboardState.noteOff(1, editorAuditionPitch, 0.f);
        editorAuditionPitch = -1;
    }
}

void PerformanceKeyboardPanel::rebuildPlaybackEvents() {
    playbackEvents.clear();
    modulationEnvelope.clear();
    if (!sequence.has_value()) {
        return;
    }
    for (const auto& note : sequence->notes) {
        playbackEvents.push_back({
                note.startSeconds,
                MidiMessage::noteOn(1, note.pitch, (uint8) note.velocity) });
        playbackEvents.push_back({
                note.startSeconds + note.durationSeconds,
                MidiMessage::noteOff(1, note.pitch) });
    }
    const int envelopeController = sequence->controls.empty() ? 1
            : (std::any_of(sequence->controls.begin(), sequence->controls.end(),
                    [](const PresetMidiControl& control) { return control.controller == 1; })
                    ? 1 : sequence->controls.front().controller);
    for (const auto& control : sequence->controls) {
        if (control.controller == envelopeController) {
            modulationEnvelope.push_back(control);
            continue;
        }
        playbackEvents.push_back({
                control.timeSeconds,
                MidiMessage::controllerEvent(1, control.controller, control.value) });
    }
    std::stable_sort(playbackEvents.begin(), playbackEvents.end(),
            [](const PlaybackEvent& first, const PlaybackEvent& second) {
                if (first.timeSeconds != second.timeSeconds) {
                    return first.timeSeconds < second.timeSeconds;
                }
                const auto priority = [](const MidiMessage& message) {
                    return message.isNoteOff() ? 0 : (message.isNoteOn() ? 2 : 1);
                };
                return priority(first.message) < priority(second.message);
            });
    std::stable_sort(modulationEnvelope.begin(), modulationEnvelope.end(),
            [](const PresetMidiControl& first, const PresetMidiControl& second) {
                return first.timeSeconds < second.timeSeconds;
            });
}

bool PerformanceKeyboardPanel::startPlayback(double nowMilliseconds) {
    releaseEditorAudition();
    stopPlayback();
    stopRecording();
    nextPlaybackEvent = 0;
    nextModulationSegment = 0;
    lastSentModulation = -1;
    activePlaybackNoteCounts.fill(0);
    playbackNote = selectedPreviewNote;
    progress = 0.f;
    playbackStartedAtMilliseconds = nowMilliseconds;
    playing = true;
    playButton.setButtonText("STOP");
    sendModWheelValue();
    if (!hasSequenceNotes()) {
        keyboardState.noteOn(1, playbackNote, 0.8f);
    }
    if (sequence.has_value()) {
        dispatchSequenceEvents(0.0);
    }
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
    if (playing && !hasSequenceNotes() && playbackNote >= 0) {
        keyboardState.noteOff(1, playbackNote, 0.f);
    }
    for (int note = 0; note < 128; ++note) {
        if (activePlaybackNoteCounts[(size_t) note] > 0) {
            keyboardState.noteOff(1, note, 0.f);
            activePlaybackNoteCounts[(size_t) note] = 0;
        }
    }
    playbackNote = -1;
    playing = false;
    playButton.setButtonText("PLAY");
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
    if (sequence.has_value()) {
        dispatchSequenceEvents(elapsedSeconds);
    }
    progress = jlimit(
            0.f,
            1.f,
            (float) (elapsedSeconds / (hasSequenceNotes()
                    ? sequence->durationSeconds : (double) playbackDuration)));
    if (progress >= 1.f) {
        stopPlayback(false);
    }
    repaint(progressBounds().getSmallestIntegerContainer().expanded(2));
}

void PerformanceKeyboardPanel::dispatchSequenceEvents(double elapsedSeconds) {
    while (nextPlaybackEvent < playbackEvents.size()
            && playbackEvents[nextPlaybackEvent].timeSeconds <= elapsedSeconds) {
        const MidiMessage& message = playbackEvents[nextPlaybackEvent++].message;
        if (message.isNoteOn()) {
            const int note = message.getNoteNumber();
            if (activePlaybackNoteCounts[(size_t) note]++ == 0) {
                keyboardState.noteOn(1, note, message.getFloatVelocity());
            }
        } else if (message.isNoteOff()) {
            const int note = message.getNoteNumber();
            if (activePlaybackNoteCounts[(size_t) note] > 0
                    && --activePlaybackNoteCounts[(size_t) note] == 0) {
                keyboardState.noteOff(1, note, 0.f);
            }
        } else {
            eventSink.enqueueMidiMessage(message, MidiEventSource::PerformanceKeyboard);
        }
    }
    if (modulationEnvelope.empty()) {
        return;
    }
    while (nextModulationSegment + 1 < modulationEnvelope.size()
            && modulationEnvelope[nextModulationSegment + 1].timeSeconds <= elapsedSeconds) {
        ++nextModulationSegment;
    }
    const auto& first = modulationEnvelope[nextModulationSegment];
    int value = first.value;
    if (nextModulationSegment + 1 < modulationEnvelope.size()) {
        const auto& second = modulationEnvelope[nextModulationSegment + 1];
        const double span = second.timeSeconds - first.timeSeconds;
        if (span > 0.0 && elapsedSeconds > first.timeSeconds) {
            const double fraction = jlimit(0.0, 1.0,
                    (elapsedSeconds - first.timeSeconds) / span);
            value = roundToInt(first.value + (second.value - first.value) * fraction);
        }
    }
    if (value != lastSentModulation) {
        eventSink.enqueueMidiMessage(
                MidiMessage::controllerEvent(1, first.controller, value),
                MidiEventSource::PerformanceKeyboard);
        lastSentModulation = value;
    }
}

void PerformanceKeyboardPanel::recordMidiMessage(
        const MidiMessage& message,
        double nowSeconds) {
    if (!recording || !sequence.has_value()) {
        return;
    }
    const double time = jlimit(0.0,
            PresetMidiSequence::maximumDurationSeconds - 0.2,
            nowSeconds - recordingStartedAtSeconds);
    if (message.isNoteOn()) {
        const int note = message.getNoteNumber();
        recordedNoteStarts[(size_t) note] = time;
        recordedVelocities[(size_t) note] = message.getVelocity();
    } else if (message.isNoteOff()) {
        const int note = message.getNoteNumber();
        const double start = recordedNoteStarts[(size_t) note];
        if (start >= 0.0
                && sequence->notes.size() < PresetMidiSequence::maximumEventsPerLane) {
            sequence->notes.push_back({ note, recordedVelocities[(size_t) note],
                    start, jmax(0.05, time - start) });
            recordedNoteStarts[(size_t) note] = -1.0;
        }
    } else if (message.isController()
            && sequence->controls.size() < PresetMidiSequence::maximumEventsPerLane) {
        sequence->controls.push_back({
                message.getControllerNumber(), message.getControllerValue(), time });
    }
    sequence->durationSeconds = jmax(sequence->durationSeconds, time + 0.1);
}

void PerformanceKeyboardPanel::stopRecording() {
    if (!recording) {
        return;
    }
    if (recordingChanged) {
        recordingChanged(false);
    }
    if (flushRecordingInput) {
        flushRecordingInput();
    }
    const double now = Time::getMillisecondCounterHiRes() / 1000.0;
    const double end = jlimit(0.0,
            PresetMidiSequence::maximumDurationSeconds - 0.2,
            now - recordingStartedAtSeconds);
    for (int note = 0; note < 128; ++note) {
        const double start = recordedNoteStarts[(size_t) note];
        if (start >= 0.0
                && sequence->notes.size() < PresetMidiSequence::maximumEventsPerLane) {
            sequence->notes.push_back({ note, recordedVelocities[(size_t) note],
                    start, jmax(0.05, end - start) });
        }
    }
    sequence->durationSeconds = jmax(sequence->durationSeconds, end + 0.1);
    recording = false;
    recordButton.setButtonText("REC");
    setSequence(*sequence);
    if (sequenceChanged) {
        sequenceChanged(*sequence);
    }
}

void PerformanceKeyboardPanel::releaseAllNotes() {
    stopPlayback();
    stopRecording();
    releaseEditorAudition();
    keyboard.releaseAllNotes();
}

void PerformanceKeyboardPanel::paint(Graphics& graphics) {
    const Rectangle<float> bounds = getLocalBounds().toFloat().reduced(0.75f);
    graphics.setColour(CanvasChromePalette::dockSurface.withAlpha(0.96f));
    graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::panelCornerRadius);
    const Rectangle<float> track = progressBounds();
    const float trackCornerRadius = track.getHeight() * 0.5f;
    graphics.setColour(CanvasChromePalette::strongBorder.withAlpha(0.22f));
    graphics.fillRoundedRectangle(track, trackCornerRadius);
    if (progress > 0.f) {
        graphics.setColour(CanvasChromePalette::focus.withAlpha(0.58f));
        graphics.fillRoundedRectangle(
                track.withWidth(track.getWidth() * progress),
                trackCornerRadius);
    }
}

void PerformanceKeyboardPanel::resized() {
    const bool compact = getWidth() < roundToInt(CanvasUtilityDock::preferredKeyboardWidth);
    const int panelInset = compact ? 4 : 6;
    const int buttonWidth = compact ? 25 : 28;
    const int controlGap = compact ? 2 : 4;
    const int wheelGap = compact ? 3 : 6;
    const int wheelWidth = compact ? 24 : 32;
    Rectangle<int> content = getLocalBounds().reduced(panelInset);
    Rectangle<int> actions = content.removeFromRight(compact ? 37 : 44);
    content.removeFromRight(compact ? 2 : 4);
    const int actionGap = 3;
    const int actionHeight = (actions.getHeight() - 2 * actionGap) / 3;
    playButton.setBounds(actions.removeFromTop(actionHeight));
    actions.removeFromTop(actionGap);
    recordButton.setBounds(actions.removeFromTop(actionHeight));
    actions.removeFromTop(actionGap);
    editButton.setBounds(actions);
    modWheel.setBounds(content.removeFromLeft(wheelWidth));
    content.removeFromLeft(wheelGap);
    octaveDown.setBounds(content.removeFromLeft(buttonWidth));
    content.removeFromLeft(controlGap);
    octaveUp.setBounds(content.removeFromRight(buttonWidth));
    content.removeFromRight(controlGap);
    keyboard.setBounds(content);
}

void PerformanceKeyboardPanel::openSequenceEditor() {
    if (recording) {
        stopRecording();
    }
    Component::SafePointer<PerformanceKeyboardPanel> safeThis(this);
    auto editor = std::make_unique<PresetMidiEditor>(
            sequence.value_or(PresetMidiSequence {}),
            [safeThis](PresetMidiSequence edited) {
                if (safeThis == nullptr) {
                    return;
                }
                safeThis->setSequence(edited);
                if (safeThis->sequenceChanged) {
                    safeThis->sequenceChanged(std::move(edited));
                }
            },
            [safeThis] {
                if (safeThis != nullptr) {
                    safeThis->togglePlayback();
                }
            },
            [safeThis](int pitch, int velocity, bool noteOn) {
                if (safeThis != nullptr) {
                    safeThis->auditionSequenceNote(pitch, velocity, noteOn);
                }
            },
            [safeThis]() -> std::optional<double> {
                if (safeThis == nullptr || !safeThis->playing) {
                    return std::nullopt;
                }
                const double duration = safeThis->hasSequenceNotes()
                        ? safeThis->sequence->durationSeconds
                        : (double) safeThis->playbackDuration;
                return duration * safeThis->progress;
            });
    auto* editorContent = editor.get();
    CallOutBox::launchAsynchronously(
            std::move(editor), editButton.getScreenBounds(), nullptr);
    editorContent->grabKeyboardFocus();
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

void PerformanceKeyboardPanel::timerCallback() {
    updatePlayback(Time::getMillisecondCounterHiRes());
}

void PerformanceKeyboardPanel::sendModWheelValue() {
    eventSink.enqueueMidiMessage(
            MidiMessage::controllerEvent(1, 1, modWheel.value()),
            MidiEventSource::PerformanceKeyboard);
}

}
