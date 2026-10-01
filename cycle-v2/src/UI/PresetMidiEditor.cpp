#include <algorithm>

#include "UI/PresetMidiEditor.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

using namespace juce;

PresetMidiEditor::PresetMidiEditor(
        PresetMidiSequence phrase,
        ChangeCallback callback,
        std::function<void()> togglePlayback) :
        sequence(std::move(phrase))
    ,   onChange(std::move(callback))
    ,   onTogglePlayback(std::move(togglePlayback)) {
    setName("PresetMidiEditor");
    setSize(520, 320);
    setWantsKeyboardFocus(true);
    addAndMakeVisible(clearButton);
    clearButton.onClick = [this] {
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
        lowestPitch = jlimit(0, 103, (lowest + highest - 23) / 2);
    }
}

Rectangle<float> PresetMidiEditor::noteGrid() const {
    return getLocalBounds().toFloat().withTrimmedLeft(46.f)
            .withTrimmedRight(14.f).withTrimmedTop(36.f).withTrimmedBottom(80.f);
}

Rectangle<float> PresetMidiEditor::controlGrid() const {
    return noteGrid().withY(noteGrid().getBottom() + 16.f).withHeight(42.f);
}

int PresetMidiEditor::pitchAt(float y) const {
    const auto grid = noteGrid();
    const int row = jlimit(0, 23, (int) ((y - grid.getY()) * 24.f / grid.getHeight()));
    return lowestPitch + 23 - row;
}

double PresetMidiEditor::timeAt(float x) const {
    const auto grid = noteGrid();
    const float proportion = jlimit(0.f, 1.f, (x - grid.getX()) / grid.getWidth());
    const int step = jlimit(0, 16, roundToInt(proportion * 16.f));
    return jmin(sequence.durationSeconds,
            timeOffset + visibleDuration() * (double) step / 16.0);
}

double PresetMidiEditor::visibleDuration() const {
    return jmax(0.25, sequence.durationSeconds * 0.5);
}

float PresetMidiEditor::xForTime(double time) const {
    return noteGrid().getX() + noteGrid().getWidth()
            * (float) ((time - timeOffset) / visibleDuration());
}

int PresetMidiEditor::controllerLane() const {
    if (sequence.controls.empty()) {
        return 1;
    }
    const auto modWheel = std::find_if(sequence.controls.begin(), sequence.controls.end(),
            [](const PresetMidiControl& control) { return control.controller == 1; });
    return modWheel != sequence.controls.end()
            ? 1 : sequence.controls.front().controller;
}

void PresetMidiEditor::paint(Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::dockSurface);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(FontOptions(14.f, Font::bold));
    graphics.drawText("PRESET PHRASE", 16, 7, 260, 20, Justification::centredLeft);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(9.f));
    for (int marker = 0; marker <= 4; ++marker) {
        const double seconds = timeOffset + visibleDuration() * marker / 4.0;
        const int x = roundToInt(xForTime(seconds));
        graphics.drawText(String(seconds, 1) + "s",
                x - (marker == 4 ? 28 : 14), 23, 36, 12,
                Justification::centredLeft);
    }
    paintNoteGrid(graphics);
    paintNotes(graphics);
    paintModulation(graphics);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(11.f));
    graphics.drawText("Two fingers: pan    Right-click/Delete: remove    Space: play", 46,
            getHeight() - 22, getWidth() - 60, 16, Justification::centredLeft);
}

void PresetMidiEditor::paintNoteGrid(Graphics& graphics) const {
    const auto grid = noteGrid();
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillRect(grid);
    for (int row = 0; row <= 24; ++row) {
        const int pitch = lowestPitch + 24 - row;
        const float y = grid.getY() + grid.getHeight() * row / 24.f;
        graphics.setColour(Colours::white.withAlpha(pitch % 12 == 0 ? 0.28f : 0.08f));
        graphics.drawHorizontalLine(roundToInt(y), grid.getX(), grid.getRight());
        if (row < 24 && pitch % 12 == 0) {
            graphics.setColour(CanvasChromePalette::mutedText);
            graphics.setFont(FontOptions(11.f));
            graphics.drawText(MidiMessage::getMidiNoteName(pitch, true, true, 3),
                    2, roundToInt(y), 42, 14, Justification::centredRight);
        }
    }
    for (int step = 0; step <= 16; ++step) {
        const float x = grid.getX() + grid.getWidth() * step / 16.f;
        graphics.setColour(Colours::white.withAlpha(step % 4 == 0 ? 0.25f : 0.09f));
        graphics.drawVerticalLine(roundToInt(x), grid.getY(), grid.getBottom());
    }
}

void PresetMidiEditor::paintNotes(Graphics& graphics) const {
    const auto grid = noteGrid();
    Graphics::ScopedSaveState saveState(graphics);
    graphics.reduceClipRegion(grid.getSmallestIntegerContainer());
    for (size_t index = 0; index < sequence.notes.size(); ++index) {
        const auto& note = sequence.notes[index];
        const float x = xForTime(note.startSeconds);
        const float width = jmax(3.f, grid.getWidth()
                * (float) (note.durationSeconds / visibleDuration()));
        const float y = grid.getY() + grid.getHeight()
                * (float) (lowestPitch + 23 - note.pitch) / 24.f;
        if (y >= grid.getY() && y < grid.getBottom()) {
            graphics.setColour(CanvasChromePalette::focus.withAlpha(
                    (int) index == selectedNote ? 1.f : 0.78f));
            graphics.fillRoundedRectangle({ x, y + 1.f, width, grid.getHeight() / 24.f - 2.f }, 2.f);
            if ((int) index == selectedNote) {
                graphics.setColour(Colours::white.withAlpha(0.85f));
                graphics.drawRoundedRectangle({ x, y + 1.f, width,
                        grid.getHeight() / 24.f - 2.f }, 2.f, 1.f);
            }
        }
    }
}

void PresetMidiEditor::paintModulation(Graphics& graphics) const {
    const auto controls = controlGrid();
    const int lane = controllerLane();
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(10.f));
    const String controlLabel = lane == 1 ? "MOD" : "CC" + String(lane);
    graphics.drawText(controlLabel, 4, roundToInt(controls.getY()), 38, 20,
            Justification::centredRight);
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillRect(controls);
    std::vector<size_t> modulation;
    for (size_t index = 0; index < sequence.controls.size(); ++index) {
        if (sequence.controls[index].controller == lane) {
            modulation.push_back(index);
        }
    }
    std::stable_sort(modulation.begin(), modulation.end(),
            [this](size_t first, size_t second) {
                return sequence.controls[first].timeSeconds
                        < sequence.controls[second].timeSeconds;
            });
    Graphics::ScopedSaveState saveState(graphics);
    graphics.reduceClipRegion(controls.getSmallestIntegerContainer());
    Path envelope;
    bool firstPoint = true;
    for (const size_t index : modulation) {
        const auto& control = sequence.controls[index];
        const float x = xForTime(control.timeSeconds);
        const float height = controls.getHeight() * (float) control.value / 127.f;
        const float y = controls.getBottom() - height;
        if (firstPoint) {
            envelope.startNewSubPath(x, y);
            firstPoint = false;
        } else {
            envelope.lineTo(x, y);
        }
    }
    graphics.setColour(CanvasChromePalette::focus.withAlpha(0.85f));
    graphics.strokePath(envelope, PathStrokeType(2.f));
    for (const size_t index : modulation) {
        const auto& control = sequence.controls[index];
        const float x = xForTime(control.timeSeconds);
        const float y = controls.getBottom()
                - controls.getHeight() * (float) control.value / 127.f;
        graphics.setColour((int) index == selectedControl
                ? Colours::white : CanvasChromePalette::focus);
        graphics.fillEllipse(x - 3.f, y - 3.f, 6.f, 6.f);
    }
}

void PresetMidiEditor::resized() {
    clearButton.setBounds(getWidth() - 80, 7, 60, 22);
}

void PresetMidiEditor::mouseDown(const MouseEvent& event) {
    activeNote = -1;
    activeControl = -1;
    pendingChange = false;
    if (controlGrid().contains(event.position)) {
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
    selectedControl = -1;
    const int pitch = pitchAt(event.position.y);
    const double start = timeAt(event.position.x);
    const double step = visibleDuration() / 16.0;
    const auto existing = std::find_if(sequence.notes.begin(), sequence.notes.end(),
            [pitch, start](const PresetMidiNote& note) {
                return note.pitch == pitch && start >= note.startSeconds
                        && start < note.startSeconds + note.durationSeconds;
            });
    if (existing != sequence.notes.end()) {
        selectedNote = (int) std::distance(sequence.notes.begin(), existing);
        if (event.mods.isRightButtonDown()) {
            sequence.notes.erase(existing);
            selectedNote = -1;
            publish();
            return;
        }
        activeNote = selectedNote;
        originalNote = *existing;
        gestureStart = event.position;
        const float noteRight = xForTime(
                existing->startSeconds + existing->durationSeconds);
        resizingNote = event.position.x >= noteRight - 10.f;
        repaint();
        return;
    }
    sequence.notes.push_back({ pitch, 100, start,
            jmin(step, sequence.durationSeconds - start) });
    activeNote = (int) sequence.notes.size() - 1;
    selectedNote = activeNote;
    originalNote = sequence.notes.back();
    gestureStart = event.position;
    resizingNote = true;
    pendingChange = true;
    repaint();
}

void PresetMidiEditor::mouseDrag(const MouseEvent& event) {
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
        note.durationSeconds = jlimit(sequence.durationSeconds / 16.0,
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
    }
    pendingChange = true;
    repaint();
}

void PresetMidiEditor::mouseUp(const MouseEvent&) {
    activeNote = -1;
    activeControl = -1;
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
        sequence.notes.erase(sequence.notes.begin() + selectedNote);
        selectedNote = -1;
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
    timeOffset = jlimit(0.0,
            jmax(0.0, sequence.durationSeconds - visibleDuration()),
            timeOffset - (double) (wheel.deltaX * direction) * visibleDuration());
    lowestPitch = jlimit(0, 103,
            lowestPitch + roundToInt(wheel.deltaY * direction * 12.f));
    repaint();
    (void) event;
}

void PresetMidiEditor::publish() {
    repaint();
    if (onChange) {
        onChange(sequence);
    }
}

}
