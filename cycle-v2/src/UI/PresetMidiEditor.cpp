#include <algorithm>
#include <cmath>

#include "UI/PresetMidiEditor.h"
#include "UI/CanvasChromePalette.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"

namespace CycleV2 {

using namespace juce;

PresetMidiEditor::PresetMidiEditor(
        PresetMidiSequence phrase,
        ChangeCallback callback,
        std::function<void()> togglePlayback,
        AuditionCallback auditionCallback,
        PlaybackTimeCallback playbackTimeCallback,
        std::vector<int> mappedControllers,
        SeekCallback seek, double initialPlaybackPosition) :
        sequence(std::move(phrase))
    ,   onChange(std::move(callback))
    ,   onTogglePlayback(std::move(togglePlayback))
    ,   onAudition(std::move(auditionCallback))
    ,   playbackTime(std::move(playbackTimeCallback))
    ,   onSeek(std::move(seek))
    ,   cursorSeconds(jlimit(0.0, sequence.durationSeconds, initialPlaybackPosition)) {
    setName("PresetMidiEditor");
    setSize(1120, 820);
    setWantsKeyboardFocus(true);
    addAndMakeVisible(clearButton);
    addAndMakeVisible(snapButton);
    snapButton.setName("PresetMidiEditor.Snap");
    snapButton.setToggleState(true, dontSendNotification);
    snapButton.setClickingTogglesState(true);
    snapButton.setWantsKeyboardFocus(false);
    snapButton.setMouseClickGrabsKeyboardFocus(false);
    snapButton.setTooltip("Snap note and controller timing to the grid");
    snapButton.onClick = [this] {
        if (isShowing()) {
            grabKeyboardFocus();
        }
        repaint();
    };
    addAndMakeVisible(tempoLabel);
    addAndMakeVisible(tempoSlider);
    tempoLabel.setText("BPM", dontSendNotification);
    tempoLabel.setColour(Label::textColourId, CanvasChromePalette::mutedText);
    tempoLabel.setFont(FontOptions(10.f, Font::bold));
    tempoSlider.setName("PresetMidiEditor.Tempo");
    tempoSlider.setLookAndFeel(&propertyControlLookAndFeel());
    tempoSlider.setSliderStyle(Slider::IncDecButtons);
    tempoSlider.setTextBoxStyle(Slider::TextBoxLeft, false, 56, 22);
    tempoSlider.setRange(PresetMidiSequence::minimumTempoBpm, PresetMidiSequence::maximumTempoBpm, 0.1);
    tempoSlider.setValue(sequence.tempoBpm, dontSendNotification);
    tempoSlider.setTooltip("Grid tempo; existing note and automation times stay unchanged");
    tempoSlider.onValueChange = [this] {
        sequence.tempoBpm = tempoSlider.getValue();
        timeOffsetSteps = jmin(timeOffsetSteps, maximumTimeOffsetSteps());
        publish();
    };
    addAndMakeVisible(durationLabel);
    addAndMakeVisible(durationSlider);
    durationSlider.setName("PresetMidiEditor.Length");
    durationSlider.setLookAndFeel(&propertyControlLookAndFeel());
    durationLabel.setText("LENGTH", dontSendNotification);
    durationLabel.setColour(Label::textColourId, CanvasChromePalette::mutedText);
    durationLabel.setFont(FontOptions(10.f, Font::bold));
    durationSlider.setSliderStyle(Slider::LinearHorizontal);
    durationSlider.setTextBoxStyle(Slider::TextBoxRight, false, 56, 22);
    durationSlider.setColour(Slider::trackColourId, CanvasChromePalette::focus);
    durationSlider.setColour(Slider::backgroundColourId,
            CanvasChromePalette::restingControlSurface);
    durationSlider.setColour(Slider::textBoxTextColourId,
            CanvasChromePalette::text);
    durationSlider.setTextValueSuffix(" s");
    configureControllers(std::move(mappedControllers));
    updateDurationRange();
    durationSlider.setValue(sequence.durationSeconds, dontSendNotification);
    durationSlider.onDragStart = [this] {
        draggingDuration = true;
        durationBeforeDrag = sequence.durationSeconds;
        if (playbackTime && playbackTime().has_value() && onTogglePlayback) {
            onTogglePlayback();
        }
    };
    durationSlider.onDragEnd = [this] {
        draggingDuration = false;
        if (sequence.durationSeconds != durationBeforeDrag) {
            publish();
        }
    };
    durationSlider.onValueChange = [this] {
        sequence.durationSeconds = jmax(
                minimumDurationSeconds, durationSlider.getValue());
        durationSlider.setValue(sequence.durationSeconds, dontSendNotification);
        cursorSeconds = jmin(cursorSeconds, sequence.durationSeconds);
        timeOffsetSteps = jmin(timeOffsetSteps, maximumTimeOffsetSteps());
        if (draggingDuration) {
            repaint();
        } else {
            publish();
        }
    };
    clearButton.onClick = [this] {
        releaseAudition();
        sequence.notes.clear();
        sequence.controls.clear();
        selectedNote = -1;
        selectedControl = -1;
        publish();
    };
    if (!sequence.notes.empty()) {
        int lowest = 127;
        int highest = 0;
        for (const auto& note : sequence.notes) {
            lowest = jmin(lowest, note.pitch);
            highest = jmax(highest, note.pitch);
        }
        lowestPitch = jlimit(0, 104, (lowest + highest - 23) / 2);
    }
    revealPlaybackCursor(cursorSeconds);
    if (playbackTime) {
        startTimerHz(30);
    }
}

PresetMidiEditor::~PresetMidiEditor() {
    durationSlider.setLookAndFeel(nullptr);
    tempoSlider.setLookAndFeel(nullptr);
    releaseAudition();
}

void PresetMidiEditor::resized() {
    clearButton.setBounds(getWidth() - 82, 8, 64, 24);
    tempoLabel.setBounds(173, 9, 34, 22);
    tempoSlider.setBounds(207, 8, 100, 24);
    snapButton.setBounds(getWidth() - 390, 8, 84, 26);
    durationLabel.setBounds(getWidth() - 293, 9, 52, 22);
    durationSlider.setBounds(getWidth() - 240, 8, 148, 24);
}

void PresetMidiEditor::configureControllers(std::vector<int> mappedControllers) {
    controllers = std::move(mappedControllers);
    for (const auto& point : sequence.controls) {
        controllers.push_back(point.controller);
    }
    if (controllers.empty()) {
        controllers.push_back(1);
    }
    std::sort(controllers.begin(), controllers.end());
    controllers.erase(std::unique(controllers.begin(), controllers.end()),
            controllers.end());
    setSize(1120, 820 + 104 * ((int) controllers.size() - 1));
}

void PresetMidiEditor::timerCallback() {
    const auto position = playbackTime ? playbackTime() : std::optional<double> {};
    if (position.has_value()) {
        cursorSeconds = jlimit(0.0, sequence.durationSeconds, *position);
        if (!draggingPlayback) {
            revealPlaybackCursor(cursorSeconds);
        }
        repaint();
    } else if (wasPlaybackActive) {
        repaint();
    }
    wasPlaybackActive = position.has_value();
}

double PresetMidiEditor::cursorPosition() const {
    const auto position = playbackTime ? playbackTime() : std::optional<double> {};
    return jlimit(0.0, sequence.durationSeconds, position.value_or(cursorSeconds));
}

void PresetMidiEditor::revealPlaybackCursor(double seconds) {
    if (seconds < timeOffset() || seconds >= timeOffset() + visibleDuration()) {
        timeOffsetSteps = jlimit(0, timeOffsetLimit(sequence.durationSeconds),
                (int) (seconds / gridStep()) - 8);
    }
}

void PresetMidiEditor::audition(int pitch, int velocity) {
    if (auditionPitch == pitch) {
        return;
    }
    releaseAudition();
    if (onAudition) {
        auditionPitch = pitch;
        onAudition(pitch, velocity, true);
    }
}

void PresetMidiEditor::releaseAudition() {
    if (auditionPitch >= 0 && onAudition) {
        onAudition(auditionPitch, 0, false);
    }
    auditionPitch = -1;
}

void PresetMidiEditor::updateDurationRange() {
    double lastEvent = 0.0;
    for (const auto& note : sequence.notes) {
        lastEvent = jmax(lastEvent, note.startSeconds + note.durationSeconds);
    }
    for (const auto& control : sequence.controls) {
        lastEvent = jmax(lastEvent, control.timeSeconds);
    }
    contentEndSeconds = lastEvent;
    minimumDurationSeconds = jmax(0.01, lastEvent);
    sequence.durationSeconds = jmax(sequence.durationSeconds, minimumDurationSeconds);
    durationSlider.setRange(0.01,
            PresetMidiSequence::maximumDurationSeconds, 0.01);
    durationSlider.setValue(sequence.durationSeconds, dontSendNotification);
}

void PresetMidiEditor::extendDurationTo(double endTime) {
    contentEndSeconds = jmax(contentEndSeconds,
            jmin(PresetMidiSequence::maximumDurationSeconds, endTime));
    sequence.durationSeconds = jmax(sequence.durationSeconds,
            jmin(PresetMidiSequence::maximumDurationSeconds, endTime));
    durationSlider.setValue(sequence.durationSeconds, dontSendNotification);
}

double PresetMidiEditor::minimumNoteDuration() const {
    return snapButton.getToggleState() ? gridStep() : 0.01;
}

bool PresetMidiEditor::transposeNotes(int semitones) {
    if (sequence.notes.empty()) {
        return true;
    }
    if (std::any_of(sequence.notes.begin(), sequence.notes.end(), [semitones](const auto& note) {
            return note.pitch + semitones < 0 || note.pitch + semitones > 127;
        })) {
        return true;
    }
    releaseAudition();
    for (auto& note : sequence.notes) {
        note.pitch += semitones;
    }
    publish();
    return true;
}

int PresetMidiEditor::noteAt(Point<float> point) const {
    const int pitch = pitchAt(point.y);
    const double time = rawTimeAt(point.x);
    for (size_t index = sequence.notes.size(); index > 0; --index) {
        const auto& note = sequence.notes[index - 1];
        if (note.pitch == pitch && time >= note.startSeconds
                && time < note.startSeconds + note.durationSeconds) {
            return (int) index - 1;
        }
    }
    return -1;
}

int PresetMidiEditor::velocityNoteAt(float x) const {
    if (selectedNote >= 0 && selectedNote < (int) sequence.notes.size()
            && std::abs(xForTime(sequence.notes[(size_t) selectedNote].startSeconds) - x)
                    <= 15.f) {
        return selectedNote;
    }
    int closest = -1;
    float distance = 15.f;
    for (size_t index = 0; index < sequence.notes.size(); ++index) {
        const float difference = std::abs(xForTime(sequence.notes[index].startSeconds) - x);
        if (difference < distance) {
            distance = difference;
            closest = (int) index;
        }
    }
    return closest;
}

void PresetMidiEditor::scrubPlayback(Point<float> point) {
    const auto map = miniMap();
    const double seconds = draggingOverview
            ? sequence.durationSeconds * jlimit(0.0, 1.0,
                    (double) (point.x - map.getX()) / map.getWidth())
            : rawTimeAt(point.x);
    seekPlayback(seconds, draggingOverview);
}

void PresetMidiEditor::seekPlayback(double seconds, bool reveal) {
    cursorSeconds = jlimit(0.0, sequence.durationSeconds, seconds);
    if (onSeek) {
        onSeek(cursorSeconds);
    }
    if (reveal) {
        revealPlaybackCursor(cursorSeconds);
    }
    repaint();
}

void PresetMidiEditor::mouseMove(const MouseEvent& event) {
    if (miniMap().contains(event.position) || playbackRuler().contains(event.position)) {
        setMouseCursor(MouseCursor::LeftRightResizeCursor);
        return;
    }
    if (velocityGrid().contains(event.position)
            && velocityNoteAt(event.position.x) >= 0) {
        setMouseCursor(MouseCursor::UpDownResizeCursor);
        return;
    }
    if (noteGrid().contains(event.position)) {
        const int index = noteAt(event.position);
        if (index >= 0) {
            const auto& note = sequence.notes[(size_t) index];
            const float right = xForTime(note.startSeconds + note.durationSeconds);
            setMouseCursor(event.position.x >= right - 12.f
                    ? MouseCursor::LeftRightResizeCursor
                    : MouseCursor::DraggingHandCursor);
            return;
        }
    }
    setMouseCursor(MouseCursor::NormalCursor);
}

void PresetMidiEditor::mouseDown(const MouseEvent& event) {
    if (isShowing()) {
        grabKeyboardFocus();
    }
    activeNote = -1;
    activeVelocity = -1;
    activeControl = -1;
    draggingPlayback = false;
    pendingChange = false;
    if (!event.mods.isRightButtonDown()
            && (miniMap().contains(event.position) || playbackRuler().contains(event.position))) {
        releaseAudition();
        draggingPlayback = true;
        draggingOverview = miniMap().contains(event.position);
        scrubPlayback(event.position);
        return;
    }
    if (velocityGrid().contains(event.position)) {
        const int index = velocityNoteAt(event.position.x);
        if (index >= 0 && !event.mods.isRightButtonDown()) {
            activeVelocity = index;
            selectedNote = index;
            selectedControl = -1;
            velocitySelected = true;
            gestureStart = event.position;
            originalNote = sequence.notes[(size_t) index];
            auto& note = sequence.notes[(size_t) index];
            note.velocity = velocityAt(event.position.y);
            pendingChange = note.velocity != originalNote.velocity;
            audition(note.pitch, note.velocity);
            repaint();
        }
        return;
    }
    const int controller = controllerAt(event.position);
    if (controller >= 0) {
        velocitySelected = false;
        const auto grid = controlGrid(controller);
        for (size_t index = 0; index < sequence.controls.size(); ++index) {
            const auto& control = sequence.controls[index];
            const float x = xForTime(control.timeSeconds);
            const float y = grid.getBottom() - grid.getHeight()
                    * (float) control.value / 127.f;
            if (control.controller == controller
                    && std::abs(x - event.position.x) <= 8.f
                    && std::abs(y - event.position.y) <= 8.f) {
                selectedControl = (int) index;
                if (event.mods.isRightButtonDown()) {
                    sequence.controls.erase(sequence.controls.begin() + selectedControl);
                    selectedControl = -1;
                    publish();
                } else {
                    activeControl = selectedControl;
                    repaint();
                }
                return;
            }
        }
        if (event.mods.isRightButtonDown()) {
            return;
        }
        editControl(event.position, controller);
        return;
    }
    if (!noteGrid().contains(event.position)) {
        return;
    }
    velocitySelected = false;
    selectedControl = -1;
    const int pitch = pitchAt(event.position.y);
    const int existing = noteAt(event.position);
    if (existing >= 0) {
        selectedNote = existing;
        if (event.mods.isRightButtonDown()) {
            sequence.notes.erase(sequence.notes.begin() + existing);
            selectedNote = -1;
            publish();
            return;
        }
        activeNote = existing;
        originalNote = sequence.notes[(size_t) existing];
        gestureStart = event.position;
        const float noteRight = xForTime(
                originalNote.startSeconds + originalNote.durationSeconds);
        resizingNote = event.position.x >= noteRight - 12.f;
        audition(originalNote.pitch, originalNote.velocity);
        repaint();
        return;
    }
    if (event.mods.isRightButtonDown()) {
        return;
    }
    const double duration = lastEditedNote.has_value() ? lastEditedNote->durationSeconds : gridStep();
    const int velocity = lastEditedNote.has_value() ? lastEditedNote->velocity : 100;
    const double start = jmin(timeAt(event.position.x),
            PresetMidiSequence::maximumDurationSeconds - duration);
    sequence.notes.push_back({ pitch, velocity, start, duration });
    extendDurationTo(start + duration);
    activeNote = (int) sequence.notes.size() - 1;
    selectedNote = activeNote;
    originalNote = sequence.notes.back();
    gestureStart = event.position;
    resizingNote = false;
    pendingChange = true;
    audition(pitch, sequence.notes.back().velocity);
    repaint();
}

void PresetMidiEditor::mouseDrag(const MouseEvent& event) {
    if (draggingPlayback) {
        scrubPlayback(event.position);
        return;
    }
    if (activeVelocity >= 0) {
        auto& note = sequence.notes[(size_t) activeVelocity];
        note.velocity = event.mods.isShiftDown()
                ? jlimit(1, 127, originalNote.velocity
                        + roundToInt((gestureStart.y - event.position.y) * 0.5f))
                : velocityAt(event.position.y);
        pendingChange = pendingChange || note.velocity != originalNote.velocity;
        repaint();
        return;
    }
    if (activeControl >= 0) {
        auto& control = sequence.controls[(size_t) activeControl];
        const auto grid = controlGrid(control.controller);
        control.value = jlimit(0, 127, roundToInt(127.f
                * (grid.getBottom() - event.position.y)
                / grid.getHeight()));
        control.timeSeconds = timeAt(event.position.x);
        pendingChange = true;
        repaint();
        return;
    }
    if (activeNote < 0) {
        return;
    }
    auto& note = sequence.notes[(size_t) activeNote];
    if (resizingNote) {
        note.durationSeconds = jlimit(jmin(minimumNoteDuration(),
                        PresetMidiSequence::maximumDurationSeconds - note.startSeconds),
                PresetMidiSequence::maximumDurationSeconds - note.startSeconds,
                originalNote.durationSeconds
                        + timeAt(event.position.x) - timeAt(gestureStart.x));
    } else {
        note.pitch = jlimit(0, 127, originalNote.pitch
                + pitchAt(event.position.y) - pitchAt(gestureStart.y));
        note.startSeconds = jlimit(0.0,
                PresetMidiSequence::maximumDurationSeconds - originalNote.durationSeconds,
                originalNote.startSeconds
                        + timeAt(event.position.x) - timeAt(gestureStart.x));
        audition(note.pitch, note.velocity);
    }
    extendDurationTo(note.startSeconds + note.durationSeconds);
    pendingChange = true;
    repaint();
}

void PresetMidiEditor::mouseUp(const MouseEvent&) {
    const int edited = activeVelocity >= 0 ? activeVelocity : activeNote;
    if (edited >= 0 && edited < (int) sequence.notes.size()) {
        lastEditedNote = sequence.notes[(size_t) edited];
    }
    releaseAudition();
    activeNote = -1;
    activeVelocity = -1;
    activeControl = -1;
    draggingPlayback = false;
    if (pendingChange) {
        pendingChange = false;
        publish();
    }
}

int PresetMidiEditor::controllerAt(Point<float> point) const {
    for (const int controller : controllers) {
        if (controlGrid(controller).contains(point)) {
            return controller;
        }
    }
    return -1;
}

void PresetMidiEditor::editControl(Point<float> point, int controller) {
    const auto grid = controlGrid(controller);
    if (!grid.contains(point)) {
        return;
    }
    const int value = jlimit(0, 127, roundToInt(127.f
            * (grid.getBottom() - point.y) / grid.getHeight()));
    const double time = timeAt(point.x);
    auto existing = std::find_if(sequence.controls.begin(), sequence.controls.end(),
            [controller, time](const PresetMidiControl& control) {
                return control.controller == controller && control.timeSeconds == time;
            });
    if (existing == sequence.controls.end()) {
        sequence.controls.push_back({ controller, value, time });
        selectedControl = (int) sequence.controls.size() - 1;
    } else {
        existing->value = value;
        selectedControl = (int) std::distance(sequence.controls.begin(), existing);
    }
    selectedNote = -1;
    activeControl = selectedControl;
    pendingChange = true;
    repaint();
}

bool PresetMidiEditor::keyPressed(const KeyPress& key) {
    if (key == KeyPress::leftKey || key == KeyPress::rightKey) {
        releaseAudition();
        const double measure = gridStep() * 16.0;
        seekPlayback(cursorPosition() + (key == KeyPress::rightKey ? measure : -measure), true);
        return true;
    }
    if (key == KeyPress::backspaceKey) {
        releaseAudition();
        seekPlayback(0.0, true);
        return true;
    }
    if (key.getModifiers().isShiftDown() && !key.getModifiers().isCommandDown()
            && !key.getModifiers().isCtrlDown() && !key.getModifiers().isAltDown()
            && (key.getKeyCode() == KeyPress::upKey || key.getKeyCode() == KeyPress::downKey)) {
        return transposeNotes(key.getKeyCode() == KeyPress::upKey ? 1 : -1);
    }
    if (key == KeyPress::spaceKey) {
        cursorSeconds = cursorPosition();
        if (onTogglePlayback) {
            onTogglePlayback();
        }
        return true;
    }
    if (key == KeyPress::deleteKey
            && selectedControl >= 0
            && selectedControl < (int) sequence.controls.size()) {
        sequence.controls.erase(sequence.controls.begin() + selectedControl);
        selectedControl = -1;
        publish();
        return true;
    }
    if (key == KeyPress::deleteKey
            && selectedNote >= 0 && selectedNote < (int) sequence.notes.size()) {
        releaseAudition();
        sequence.notes.erase(sequence.notes.begin() + selectedNote);
        selectedNote = -1;
        publish();
        return true;
    }
    if (velocitySelected && selectedNote >= 0
            && selectedNote < (int) sequence.notes.size()
            && (key == KeyPress::upKey || key == KeyPress::downKey)) {
        auto& velocity = sequence.notes[(size_t) selectedNote].velocity;
        velocity = jlimit(1, 127,
                velocity + (key == KeyPress::upKey ? 1 : -1));
        lastEditedNote = sequence.notes[(size_t) selectedNote];
        publish();
        return true;
    }
    if (key == KeyPress::escapeKey) {
        releaseAudition();
        if (auto* dialog = findParentComponentOfClass<DialogWindow>()) {
            dialog->setVisible(false);
        }
        return true;
    }
    return false;
}

void PresetMidiEditor::mouseWheelMove(
        const MouseEvent& event,
        const MouseWheelDetails& wheel) {
    const float direction = wheel.isReversed ? -1.f : 1.f;
    horizontalWheelRemainder -= wheel.deltaX * direction * 16.f;
    verticalWheelRemainder += wheel.deltaY * direction * 36.f;
    const int horizontalSteps = (int) horizontalWheelRemainder;
    const int verticalRows = (int) verticalWheelRemainder;
    horizontalWheelRemainder -= horizontalSteps;
    verticalWheelRemainder -= verticalRows;
    const int nextTime = jlimit(0, maximumTimeOffsetSteps(),
            timeOffsetSteps + horizontalSteps);
    const int nextPitch = jlimit(0, 104, lowestPitch + verticalRows);
    if (nextTime != timeOffsetSteps || nextPitch != lowestPitch) {
        timeOffsetSteps = nextTime;
        lowestPitch = nextPitch;
        repaint();
    }
    if ((nextTime == 0 && horizontalWheelRemainder < 0.f)
            || (nextTime == maximumTimeOffsetSteps()
                    && horizontalWheelRemainder > 0.f)) {
        horizontalWheelRemainder = 0.f;
    }
    if ((nextPitch == 0 && verticalWheelRemainder < 0.f)
            || (nextPitch == 104 && verticalWheelRemainder > 0.f)) {
        verticalWheelRemainder = 0.f;
    }
    (void) event;
}

void PresetMidiEditor::publish() {
    updateDurationRange();
    cursorSeconds = cursorPosition();
    repaint();
    if (onChange) {
        onChange(sequence);
    }
}

}
