#include <algorithm>
#include <cmath>

#include "UI/PresetMidiEditor.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

using namespace juce;

PresetMidiEditor::PresetMidiEditor(
        PresetMidiSequence phrase,
        ChangeCallback callback,
        std::function<void()> togglePlayback,
        AuditionCallback auditionCallback,
        PlaybackTimeCallback playbackTimeCallback) :
        sequence(std::move(phrase))
    ,   onChange(std::move(callback))
    ,   onTogglePlayback(std::move(togglePlayback))
    ,   onAudition(std::move(auditionCallback))
    ,   playbackTime(std::move(playbackTimeCallback)) {
    setName("PresetMidiEditor");
    setSize(840, 640);
    setWantsKeyboardFocus(true);
    addAndMakeVisible(clearButton);
    addAndMakeVisible(durationLabel);
    addAndMakeVisible(durationSlider);
    durationSlider.setName("PresetMidiEditor.Length");
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
    if (playbackTime) {
        startTimerHz(30);
    }
}

PresetMidiEditor::~PresetMidiEditor() {
    releaseAudition();
}

void PresetMidiEditor::resized() {
    clearButton.setBounds(getWidth() - 82, 8, 64, 24);
    durationLabel.setBounds(getWidth() - 293, 9, 52, 22);
    durationSlider.setBounds(getWidth() - 240, 8, 148, 24);
}

void PresetMidiEditor::timerCallback() {
    const auto position = playbackTime ? playbackTime() : std::optional<double> {};
    if (position.has_value()) {
        if (*position >= timeOffset() + visibleDuration()
                && timeOffsetSteps < maximumTimeOffsetSteps()) {
            timeOffsetSteps = jlimit(0, maximumTimeOffsetSteps(),
                    (int) (*position / gridStep()) - 12);
        }
        repaint();
    } else if (wasPlaybackActive) {
        repaint();
    }
    wasPlaybackActive = position.has_value();
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
    double lastEvent = 0.01;
    for (const auto& note : sequence.notes) {
        lastEvent = jmax(lastEvent, note.startSeconds + note.durationSeconds);
    }
    for (const auto& control : sequence.controls) {
        lastEvent = jmax(lastEvent, control.timeSeconds);
    }
    minimumDurationSeconds = lastEvent;
    sequence.durationSeconds = jmax(sequence.durationSeconds, lastEvent);
    durationSlider.setRange(0.01,
            PresetMidiSequence::maximumDurationSeconds, 0.01);
    durationSlider.setValue(sequence.durationSeconds, dontSendNotification);
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

void PresetMidiEditor::navigateMiniMap(Point<float> point) {
    const auto map = miniMap();
    const auto pitches = overviewPitches();
    const float x = jlimit(0.f, 1.f,
            (point.x - map.getX()) / map.getWidth());
    const float y = jlimit(0.f, 1.f,
            (point.y - map.getY()) / map.getHeight());
    const double centreTime = sequence.durationSeconds * x;
    timeOffsetSteps = jlimit(0, maximumTimeOffsetSteps(),
            roundToInt((centreTime - visibleDuration() * 0.5) / gridStep()));
    const int centrePitch = roundToInt(pitches.getEnd()
            - pitches.getLength() * y);
    lowestPitch = jlimit(0, 104, centrePitch - 12);
    horizontalWheelRemainder = 0.f;
    verticalWheelRemainder = 0.f;
    repaint();
}

void PresetMidiEditor::mouseMove(const MouseEvent& event) {
    if (miniMap().contains(event.position)) {
        setMouseCursor(MouseCursor::DraggingHandCursor);
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
    activeNote = -1;
    activeVelocity = -1;
    activeControl = -1;
    draggingMiniMap = false;
    pendingChange = false;
    if (miniMap().contains(event.position)) {
        draggingMiniMap = true;
        navigateMiniMap(event.position);
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
    if (controlGrid().contains(event.position)) {
        velocitySelected = false;
        const auto grid = controlGrid();
        const int lane = controllerLane();
        for (size_t index = 0; index < sequence.controls.size(); ++index) {
            const auto& control = sequence.controls[index];
            const float x = xForTime(control.timeSeconds);
            const float y = grid.getBottom() - grid.getHeight()
                    * (float) control.value / 127.f;
            if (control.controller == lane
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
        editControl(event.position);
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
    const double step = jmin(gridStep(), sequence.durationSeconds);
    const double start = jmin(timeAt(event.position.x),
            sequence.durationSeconds - step);
    sequence.notes.push_back({ pitch, 100, start,
            jmin(step, sequence.durationSeconds - start) });
    activeNote = (int) sequence.notes.size() - 1;
    selectedNote = activeNote;
    originalNote = sequence.notes.back();
    gestureStart = event.position;
    resizingNote = true;
    pendingChange = true;
    audition(pitch, sequence.notes.back().velocity);
    repaint();
}

void PresetMidiEditor::mouseDrag(const MouseEvent& event) {
    if (draggingMiniMap) {
        navigateMiniMap(event.position);
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
        control.value = jlimit(0, 127, roundToInt(127.f
                * (controlGrid().getBottom() - event.position.y)
                / controlGrid().getHeight()));
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
        note.durationSeconds = jlimit(jmin(gridStep(),
                        sequence.durationSeconds - note.startSeconds),
                sequence.durationSeconds - note.startSeconds,
                originalNote.durationSeconds
                        + timeAt(event.position.x) - timeAt(gestureStart.x));
    } else {
        note.pitch = jlimit(0, 127, originalNote.pitch
                + pitchAt(event.position.y) - pitchAt(gestureStart.y));
        note.startSeconds = jlimit(0.0,
                sequence.durationSeconds - originalNote.durationSeconds,
                originalNote.startSeconds
                        + timeAt(event.position.x) - timeAt(gestureStart.x));
        audition(note.pitch, note.velocity);
    }
    pendingChange = true;
    repaint();
}

void PresetMidiEditor::mouseUp(const MouseEvent&) {
    releaseAudition();
    activeNote = -1;
    activeVelocity = -1;
    activeControl = -1;
    draggingMiniMap = false;
    if (pendingChange) {
        pendingChange = false;
        publish();
    }
}

void PresetMidiEditor::editControl(Point<float> point) {
    if (!controlGrid().contains(point)) {
        return;
    }
    const int value = jlimit(0, 127, roundToInt(127.f
            * (controlGrid().getBottom() - point.y) / controlGrid().getHeight()));
    const double time = timeAt(point.x);
    const int lane = controllerLane();
    auto existing = std::find_if(sequence.controls.begin(), sequence.controls.end(),
            [lane, time](const PresetMidiControl& control) {
                return control.controller == lane && control.timeSeconds == time;
            });
    if (existing == sequence.controls.end()) {
        sequence.controls.push_back({ lane, value, time });
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
    if (key == KeyPress::spaceKey) {
        if (onTogglePlayback) {
            onTogglePlayback();
        }
        return true;
    }
    if ((key == KeyPress::deleteKey || key == KeyPress::backspaceKey)
            && selectedControl >= 0
            && selectedControl < (int) sequence.controls.size()) {
        sequence.controls.erase(sequence.controls.begin() + selectedControl);
        selectedControl = -1;
        publish();
        return true;
    }
    if ((key == KeyPress::deleteKey || key == KeyPress::backspaceKey)
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
        publish();
        return true;
    }
    if (key == KeyPress::escapeKey) {
        if (auto* callout = findParentComponentOfClass<CallOutBox>()) {
            callout->dismiss();
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
    repaint();
    if (onChange) {
        onChange(sequence);
    }
}

}
