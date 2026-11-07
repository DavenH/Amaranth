#include <algorithm>

#include "UI/PerformanceKeyboard.h"
#include "UI/CanvasChromePalette.h"
#include "UI/PresetMidiEditor.h"

namespace CycleV2 {

namespace {

constexpr double automationLookaheadSeconds = 0.5;

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
    addAndMakeVisible(waveform);
    addAndMakeVisible(octaveDown);
    addAndMakeVisible(octaveUp);
    addAndMakeVisible(controllerViewport);
    controllerViewport.setViewedComponent(&controllerStrip, false);
    controllerViewport.setScrollBarsShown(false, true, false, true);
    controllerViewport.setScrollBarThickness(3);
    controllerStrip.setInterceptsMouseClicks(false, true);
    controllerStrip.addAndMakeVisible(modWheel);
    addAndMakeVisible(playButton);
    addAndMakeVisible(editButton);
    addAndMakeVisible(recordButton);
    playButton.setName("PerformanceKeyboard.PlaySequence");
    editButton.setName("PerformanceKeyboard.EditSequence");
    recordButton.setName("PerformanceKeyboard.RecordSequence");
    playButton.setTooltip("Play or stop this preset preview phrase");
    editButton.setTooltip("Edit preset preview notes and modulation");
    recordButton.setTooltip("Record MIDI input into this preset preview");
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
        recordButton.setToggleState(true, dontSendNotification);
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
        if (controllerValueChanged) {
            controllerValueChanged(1, value);
        } else {
            sendModWheelValue();
        }
        if (modWheelValueChanged) {
            modWheelValueChanged(value);
        }
    };
    modWheel.onGestureStarted = [this] {
        if (modWheelGestureStarted) {
            modWheelGestureStarted();
        }
        if (controllerGestureStarted) {
            controllerGestureStarted(1);
        }
    };
    modWheel.onGestureEnded = [this] {
        if (modWheelGestureEnded) {
            modWheelGestureEnded();
        }
        if (controllerGestureEnded) {
            controllerGestureEnded(1);
        }
    };
    keyboard.setPrimaryGestureStartedCallback([this] { stopPlayback(); });
    keyboard.setHighlightedNote(selectedPreviewNote);
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
    return getLocalArea(&modWheel, modWheel.getLocalBounds()).toFloat();
}

Rectangle<float> PerformanceKeyboardPanel::controllerBounds(int controller) const {
    if (controller == 1) {
        return modWheelBounds();
    }
    for (const auto& wheel : extraWheels) {
        if (wheel->controller() == controller) {
            return getLocalArea(wheel.get(), wheel->getLocalBounds()).toFloat()
                    .getIntersection(controllerViewport.getBounds().toFloat());
        }
    }
    return {};
}

int PerformanceKeyboardPanel::controllerValue(int controller) const {
    if (controllerValueProvider) {
        return controllerValueProvider(controller);
    }
    if (controller == 1) {
        return modWheel.value();
    }
    for (const auto& wheel : extraWheels) {
        if (wheel->controller() == controller) {
            return wheel->value();
        }
    }
    return 0;
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

void PerformanceKeyboardPanel::setControllerValueChangedCallback(
        std::function<void(int, int)> callback) {
    controllerValueChanged = std::move(callback);
}

void PerformanceKeyboardPanel::setControllerGestureStartedCallback(
        std::function<void(int)> callback) {
    controllerGestureStarted = std::move(callback);
}

void PerformanceKeyboardPanel::setControllerGestureEndedCallback(
        std::function<void(int)> callback) {
    controllerGestureEnded = std::move(callback);
}

void PerformanceKeyboardPanel::setModWheelValue(int value) {
    modWheel.setValue(value, true);
}

void PerformanceKeyboardPanel::reflectControllerValue(int controller, int value) {
    if (controller == 1) {
        modWheel.setValue(value, false);
        return;
    }
    for (const auto& wheel : extraWheels) {
        if (wheel->controller() == controller) {
            wheel->setValue(value, false);
            return;
        }
    }
}

void PerformanceKeyboardPanel::setPlaybackDurationSeconds(float seconds) {
    playbackDuration = jmax(0.001f, seconds);
}

void PerformanceKeyboardPanel::setSequence(std::optional<PresetMidiSequence> nextSequence) {
    stopPlayback();
    stopRecording();
    releaseEditorAudition();
    usePatternDuration = false;
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

void PerformanceKeyboardPanel::setMappedControllers(std::vector<int> controllers) {
    std::sort(controllers.begin(), controllers.end());
    controllers.erase(std::unique(controllers.begin(), controllers.end()),
            controllers.end());
    if (controllers == mappedControllers) {
        return;
    }
    mappedControllers = std::move(controllers);
    std::vector<std::unique_ptr<ModWheel>> nextWheels;
    for (const int controller : mappedControllers) {
        if (controller == 1) {
            continue;
        }
        auto wheel = std::make_unique<ModWheel>(controller);
        wheel->setValue(controllerValue(controller), false);
        wheel->setTooltip("Preview MIDI CC " + String(controller));
        wheel->onValueChanged = [this, controller](int value) {
            if (controllerValueChanged) {
                controllerValueChanged(controller, value);
            } else {
                sendControllerValue(controller, value);
            }
        };
        wheel->onGestureStarted = [this, controller] {
            if (controllerGestureStarted) {
                controllerGestureStarted(controller);
            }
        };
        wheel->onGestureEnded = [this, controller] {
            if (controllerGestureEnded) {
                controllerGestureEnded(controller);
            }
        };
        controllerStrip.addAndMakeVisible(*wheel);
        nextWheels.push_back(std::move(wheel));
    }
    extraWheels = std::move(nextWheels);
    controllerViewport.setViewPosition(0, 0);
    resized();
}

bool PerformanceKeyboardPanel::hasSequenceNotes() const {
    return sequence.has_value() && !sequence->notes.empty();
}

void PerformanceKeyboardPanel::auditionSequenceNote(
        int pitch, int velocity, bool noteOn) {
    if (noteOn) {
        stopPlayback(false);
        releaseEditorAudition();
        eventSink.enqueueMidiMessage(MidiMessage::allSoundOff(1), MidiEventSource::EditorAudition);
        editorAuditionPitch = pitch;
        setPreviewNote(pitch);
        eventSink.enqueueMidiMessage(MidiMessage::noteOn(1, pitch, (uint8) velocity),
                MidiEventSource::EditorAudition);
        keyboard.mirrorSequenceNote(pitch, (float) velocity / 127.f, true);
    } else if (editorAuditionPitch == pitch) {
        releaseEditorAudition();
    }
}

void PerformanceKeyboardPanel::releaseEditorAudition() {
    if (editorAuditionPitch >= 0) {
        keyboard.mirrorSequenceNote(editorAuditionPitch, 0.f, false);
        eventSink.enqueueMidiMessage(MidiMessage::noteOff(1, editorAuditionPitch),
                MidiEventSource::EditorAudition);
        editorAuditionPitch = -1;
    }
}

void PerformanceKeyboardPanel::rebuildPlaybackEvents(double startSeconds) {
    playbackEvents.clear();
    controlSchedulers.clear();
    if (!sequence.has_value()) {
        return;
    }
    auto prepared = PatternPlaybackSchedule::prepare(*sequence, startSeconds);
    playbackEvents = std::move(prepared.notes);
    controlSchedulers = std::move(prepared.controls);
}

double PerformanceKeyboardPanel::playbackLength() const {
    return sequence.has_value() && (hasSequenceNotes() || usePatternDuration)
            ? sequence->durationSeconds : (double) playbackDuration;
}

bool PerformanceKeyboardPanel::startPlayback(double nowMilliseconds) {
    const double startPosition = playbackPosition < playbackLength() ? playbackPosition : 0.0;
    releaseEditorAudition();
    stopPlayback(false);
    stopRecording();
    nextPlaybackEvent = 0;
    rebuildPlaybackEvents(startPosition);
    activePlaybackNoteCounts.fill(0);
    playbackNote = selectedPreviewNote;
    playbackPosition = startPosition;
    progress = (float) (startPosition / playbackLength());
    playbackStartedAtMilliseconds = nowMilliseconds - startPosition * 1000.0;
    const double scheduleOrigin = playbackStartedAtMilliseconds / 1000.0;
    playing = true;
    playButton.setToggleState(true, dontSendNotification);
    const auto seedController = [&](int controller) {
        return eventSink.enqueueMidiMessageAt(
                MidiMessage::controllerEvent(1, controller, controllerValue(controller)),
                MidiEventSource::PerformanceKeyboard, nowMilliseconds / 1000.0);
    };
    bool seeded = seedController(1);
    for (const auto& wheel : extraWheels) {
        seeded = seedController(wheel->controller()) && seeded;
    }
    if (!seeded) {
        stopPlayback();
        return false;
    }
    if (!scheduleControlEventsUntil(startPosition + automationLookaheadSeconds, scheduleOrigin)) {
        stopPlayback();
        return false;
    }
    if (hasSequenceNotes() && !scheduleSequenceEvents(scheduleOrigin)) {
        stopPlayback();
        return false;
    }
    if (!hasSequenceNotes()) {
        keyboardState.noteOn(1, playbackNote, 0.8f);
    }
    if (sequence.has_value()) {
        dispatchSequenceEvents(startPosition);
    }
    startTimerHz(60);
    repaint();
    return true;
}

void PerformanceKeyboardPanel::seekPlayback(double seconds, double nowMilliseconds) {
    const bool resume = playing;
    stopPlayback(false);
    usePatternDuration = sequence.has_value();
    playbackPosition = jlimit(0.0, playbackLength(), seconds);
    progress = jlimit(0.f, 1.f, (float) (playbackPosition / playbackLength()));
    if (resume && playbackPosition < playbackLength()) {
        startPlayback(nowMilliseconds);
    }
    repaint();
}

void PerformanceKeyboardPanel::togglePlayback() {
    if (playing) {
        updatePlayback(Time::getMillisecondCounterHiRes());
        stopPlayback(false);
        return;
    }
    startPlayback(Time::getMillisecondCounterHiRes());
}

void PerformanceKeyboardPanel::stopPlayback(bool resetProgress) {
    stopTimer();
    if (playing && sequence.has_value()) {
        eventSink.releaseMidiSource(MidiEventSource::PatternPlayback);
    }
    if (playing && !hasSequenceNotes() && playbackNote >= 0) {
        keyboardState.noteOff(1, playbackNote, 0.f);
    }
    for (int note = 0; note < 128; ++note) {
        if (activePlaybackNoteCounts[(size_t) note] > 0) {
            keyboard.mirrorSequenceNote(note, 0.f, false);
            activePlaybackNoteCounts[(size_t) note] = 0;
        }
    }
    playbackNote = -1;
    playing = false;
    playButton.setToggleState(false, dontSendNotification);
    if (resetProgress) {
        progress = 0.f;
        playbackPosition = 0.0;
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
        if (!scheduleControlEventsUntil(
                elapsedSeconds + automationLookaheadSeconds,
                playbackStartedAtMilliseconds / 1000.0)) {
            stopPlayback();
            return;
        }
    }
    playbackPosition = jmin(playbackLength(), elapsedSeconds);
    progress = (float) (playbackPosition / playbackLength());
    if (progress >= 1.f) {
        stopPlayback(false);
    }
}

void PerformanceKeyboardPanel::dispatchSequenceEvents(double elapsedSeconds) {
    while (nextPlaybackEvent < playbackEvents.size()
            && playbackEvents[nextPlaybackEvent].timeSeconds <= elapsedSeconds) {
        const MidiMessage& message = playbackEvents[nextPlaybackEvent++].message;
        if (message.isNoteOn()) {
            const int note = message.getNoteNumber();
            if (activePlaybackNoteCounts[(size_t) note]++ == 0) {
                keyboard.mirrorSequenceNote(note, message.getFloatVelocity(), true);
            }
        } else if (message.isNoteOff()) {
            const int note = message.getNoteNumber();
            if (activePlaybackNoteCounts[(size_t) note] > 0
                    && --activePlaybackNoteCounts[(size_t) note] == 0) {
                keyboard.mirrorSequenceNote(note, 0.f, false);
            }
        }
    }
}

bool PerformanceKeyboardPanel::scheduleSequenceEvents(double startSeconds) {
    for (const auto& event : playbackEvents) {
        if (!eventSink.enqueueMidiMessageAt(
                event.message,
                MidiEventSource::PatternPlayback,
                startSeconds + event.timeSeconds)) {
            eventSink.releaseMidiSource(MidiEventSource::PatternPlayback);
            return false;
        }
    }
    return true;
}

bool PerformanceKeyboardPanel::scheduleControlEventsUntil(
        double elapsedSeconds, double startSeconds) {
    for (auto& scheduler : controlSchedulers) {
        if (!scheduler.scheduleUntil(elapsedSeconds, startSeconds, eventSink)) {
            return false;
        }
    }
    return true;
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
    recordButton.setToggleState(false, dontSendNotification);
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

void PerformanceKeyboardPanel::openSequenceEditor() {
    showSequenceEditor(sequence.value_or(PresetMidiSequence {}),
            [safeThis = Component::SafePointer<PerformanceKeyboardPanel>(this)](
                    PresetMidiSequence edited) {
                if (safeThis != nullptr && safeThis->sequenceChanged) {
                    safeThis->sequenceChanged(std::move(edited));
                }
            });
}

void PerformanceKeyboardPanel::showSequenceEditor(
        PresetMidiSequence initial,
        std::function<void(PresetMidiSequence)> onEdit) {
    if (recording) {
        stopRecording();
    }
    Component::SafePointer<PerformanceKeyboardPanel> safeThis(this);
    auto editor = std::make_unique<PresetMidiEditor>(
            std::move(initial),
            [safeThis, onEdit = std::move(onEdit)](PresetMidiSequence edited) {
                if (safeThis == nullptr) {
                    return;
                }
                const double position = safeThis->playbackPosition;
                safeThis->setSequence(edited);
                if (onEdit) {
                    onEdit(std::move(edited));
                }
                if (safeThis != nullptr) {
                    safeThis->seekPlayback(position, Time::getMillisecondCounterHiRes());
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
                return safeThis->playbackPosition;
            },
            mappedControllers,
            [safeThis](double seconds) {
                if (safeThis != nullptr) {
                    safeThis->seekPlayback(seconds, Time::getMillisecondCounterHiRes());
                }
            }, playbackPosition);
    auto* editorContent = editor.get();
    auto* parent = getTopLevelComponent();
    const auto* display = Desktop::getInstance().getDisplays().getDisplayForRect(parent->getScreenBounds());
    if (display != nullptr) {
        editor->setSize(jmin(editor->getWidth(), display->userArea.getWidth() - 64),
                jmin(editor->getHeight(), display->userArea.getHeight() - 100));
    }
    DialogWindow::LaunchOptions options;
    options.content.setOwned(editor.release());
    options.dialogTitle = "MIDI Pattern";
    options.dialogBackgroundColour = CanvasChromePalette::dockSurface;
    options.componentToCentreAround = parent;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.escapeKeyTriggersCloseButton = true;
    auto* dialog = options.launchAsync();
    dialog->setName("PresetMidiEditor.Dialog");
    dialog->setResizeLimits(jmin(900, editorContent->getWidth()),
            jmin(640, editorContent->getHeight()),
            display != nullptr ? display->userArea.getWidth() : 1920,
            display != nullptr ? display->userArea.getHeight() : 1200);
    editorContent->grabKeyboardFocus();
}

void PerformanceKeyboardPanel::timerCallback() {
    updatePlayback(Time::getMillisecondCounterHiRes());
}

void PerformanceKeyboardPanel::sendModWheelValue() {
    sendControllerValue(1, controllerValue(1));
}

void PerformanceKeyboardPanel::sendControllerValue(int controller, int value) {
    eventSink.enqueueMidiMessage(
            MidiMessage::controllerEvent(1, controller, value),
            MidiEventSource::PerformanceKeyboard);
}

}
