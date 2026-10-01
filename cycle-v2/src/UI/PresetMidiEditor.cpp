#include <algorithm>

#include "UI/PresetMidiEditor.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

using namespace juce;

PresetMidiEditor::PresetMidiEditor(
        PresetMidiSequence phrase,
        ChangeCallback callback) :
        sequence(std::move(phrase))
    ,   onChange(std::move(callback)) {
    setName("PresetMidiEditor");
    setSize(520, 320);
    setWantsKeyboardFocus(true);
    addAndMakeVisible(clearButton);
    clearButton.onClick = [this] {
        sequence.notes.clear();
        sequence.controls.clear();
        selectedNote = -1;
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
    const int step = jlimit(0, 16, (int) (proportion * 16.f));
    return sequence.durationSeconds * (double) step / 16.0;
}

void PresetMidiEditor::paint(Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::dockSurface);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(FontOptions(14.f, Font::bold));
    graphics.drawText("PRESET PHRASE", 16, 7, 260, 20, Justification::centredLeft);
    paintNoteGrid(graphics);
    paintNotes(graphics);
    paintModulation(graphics);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(11.f));
    graphics.drawText("Drag notes to move or resize. Delete removes.", 46,
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
    for (size_t index = 0; index < sequence.notes.size(); ++index) {
        const auto& note = sequence.notes[index];
        const float x = grid.getX() + grid.getWidth()
                * (float) (note.startSeconds / sequence.durationSeconds);
        const float width = jmax(3.f, grid.getWidth()
                * (float) (note.durationSeconds / sequence.durationSeconds));
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
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(10.f));
    graphics.drawText("CC1", 4, roundToInt(controls.getY()), 38, 20,
            Justification::centredRight);
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillRect(controls);
    std::vector<PresetMidiControl> modulation;
    for (const auto& control : sequence.controls) {
        if (control.controller == 1) {
            modulation.push_back(control);
        }
    }
    std::stable_sort(modulation.begin(), modulation.end(),
            [](const PresetMidiControl& first, const PresetMidiControl& second) {
                return first.timeSeconds < second.timeSeconds;
            });
    Path envelope;
    bool firstPoint = true;
    for (const auto& control : modulation) {
        const float x = controls.getX() + controls.getWidth()
                * (float) (control.timeSeconds / sequence.durationSeconds);
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
    for (const auto& control : modulation) {
        const float x = controls.getX() + controls.getWidth()
                * (float) (control.timeSeconds / sequence.durationSeconds);
        const float y = controls.getBottom()
                - controls.getHeight() * (float) control.value / 127.f;
        graphics.fillEllipse(x - 3.f, y - 3.f, 6.f, 6.f);
    }
}

void PresetMidiEditor::resized() {
    clearButton.setBounds(getWidth() - 80, 7, 60, 22);
}

void PresetMidiEditor::mouseDown(const MouseEvent& event) {
    activeNote = -1;
    pendingChange = false;
    if (controlGrid().contains(event.position)) {
        editControl(event.position);
        return;
    }
    if (!noteGrid().contains(event.position)) {
        return;
    }
    const int pitch = pitchAt(event.position.y);
    const double start = timeAt(event.position.x);
    const double step = sequence.durationSeconds / 16.0;
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
        const float noteRight = noteGrid().getX() + noteGrid().getWidth()
                * (float) ((existing->startSeconds + existing->durationSeconds)
                        / sequence.durationSeconds);
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
    if (controlGrid().contains(event.position)) {
        editControl(event.position);
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
    auto existing = std::find_if(sequence.controls.begin(), sequence.controls.end(),
            [time](const PresetMidiControl& control) {
                return control.controller == 1 && control.timeSeconds == time;
            });
    if (existing == sequence.controls.end()) {
        sequence.controls.push_back({ 1, value, time });
    } else {
        existing->value = value;
    }
    pendingChange = true;
    repaint();
}

bool PresetMidiEditor::keyPressed(const KeyPress& key) {
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

void PresetMidiEditor::publish() {
    repaint();
    if (onChange) {
        onChange(sequence);
    }
}

}
