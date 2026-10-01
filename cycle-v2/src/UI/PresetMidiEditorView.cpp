#include <algorithm>
#include <cmath>

#include "UI/PresetMidiEditor.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

using namespace juce;

namespace {

bool isBlackKey(int pitch) {
    switch (pitch % 12) {
        case 1:
        case 3:
        case 6:
        case 8:
        case 10:
            return true;
        default:
            return false;
    }
}

}

Rectangle<float> PresetMidiEditor::miniMap() const {
    return { 62.f, 40.f, (float) getWidth() - 80.f, 42.f };
}

Rectangle<float> PresetMidiEditor::noteGrid() const {
    constexpr float top = 112.f;
    constexpr float reservedBelow = 25.f + 96.f + 70.f + 36.f;
    return { 62.f, top, (float) getWidth() - 80.f,
            jmax(180.f, (float) getHeight() - top - reservedBelow) };
}

Rectangle<float> PresetMidiEditor::velocityGrid() const {
    return { noteGrid().getX(), noteGrid().getBottom() + 16.f,
            noteGrid().getWidth(), 70.f };
}

Rectangle<float> PresetMidiEditor::controlGrid() const {
    return { noteGrid().getX(), velocityGrid().getBottom() + 18.f,
            noteGrid().getWidth(), 96.f };
}

int PresetMidiEditor::pitchAt(float y) const {
    const auto grid = noteGrid();
    const int row = jlimit(0, 23,
            (int) ((y - grid.getY()) * 24.f / grid.getHeight()));
    return lowestPitch + 23 - row;
}

int PresetMidiEditor::velocityAt(float y) const {
    return jlimit(1, 127, roundToInt(127.f
            * (velocityGrid().getBottom() - y) / velocityGrid().getHeight()));
}

double PresetMidiEditor::visibleDuration() const {
    return jmax(0.25, sequence.durationSeconds * 0.5);
}

double PresetMidiEditor::gridStep() const {
    return visibleDuration() / 16.0;
}

double PresetMidiEditor::timeOffset() const {
    return (double) timeOffsetSteps * gridStep();
}

int PresetMidiEditor::maximumTimeOffsetSteps() const {
    return jmax(0, (int) std::ceil(
            (sequence.durationSeconds - visibleDuration()) / gridStep()));
}

double PresetMidiEditor::timeAt(float x) const {
    const double time = rawTimeAt(x);
    return jlimit(0.0, sequence.durationSeconds,
            std::round(time / gridStep()) * gridStep());
}

double PresetMidiEditor::rawTimeAt(float x) const {
    const auto grid = noteGrid();
    const float proportion = jlimit(0.f, 1.f,
            (x - grid.getX()) / grid.getWidth());
    return timeOffset() + proportion * visibleDuration();
}

float PresetMidiEditor::xForTime(double time) const {
    return noteGrid().getX() + noteGrid().getWidth()
            * (float) ((time - timeOffset()) / visibleDuration());
}

Range<int> PresetMidiEditor::overviewPitches() const {
    if (sequence.notes.empty()) {
        return { 0, 128 };
    }
    int lowest = sequence.notes.front().pitch;
    int highest = lowest;
    for (const auto& note : sequence.notes) {
        lowest = jmin(lowest, note.pitch);
        highest = jmax(highest, note.pitch);
    }
    lowest = jmax(0, lowest - 12);
    highest = jmin(127, highest + 12);
    if (highest - lowest < 47) {
        const int centre = (lowest + highest) / 2;
        lowest = jlimit(0, 80, centre - 23);
        highest = lowest + 47;
    }
    return { lowest, highest + 1 };
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
    graphics.setFont(FontOptions(15.f, Font::bold));
    graphics.drawText("PRESET PHRASE", 16, 8, 220, 24,
            Justification::centredLeft);
    if (activeNote >= 0) {
        graphics.setColour(CanvasChromePalette::focus);
        graphics.setFont(FontOptions(12.f, Font::bold));
        graphics.drawText(resizingNote ? "RESIZE NOTE" : "MOVE NOTE",
                258, 9, 150, 22, Justification::centredLeft);
    } else if (activeVelocity >= 0) {
        graphics.setColour(CanvasChromePalette::navigationAccent);
        graphics.setFont(FontOptions(12.f, Font::bold));
        graphics.drawText("VELOCITY "
                        + String(sequence.notes[(size_t) activeVelocity].velocity),
                258, 9, 150, 22, Justification::centredLeft);
    } else if (selectedNote >= 0 && selectedNote < (int) sequence.notes.size()) {
        const auto& note = sequence.notes[(size_t) selectedNote];
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.setFont(FontOptions(11.f));
        graphics.drawText(MidiMessage::getMidiNoteName(note.pitch, true, true, 3)
                        + "  V" + String(note.velocity)
                        + "  " + String(note.startSeconds, 2)
                        + "s  LEN " + String(note.durationSeconds, 2) + "s",
                258, 9, 295, 22, Justification::centredLeft);
    }
    paintMiniMap(graphics);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(10.f));
    for (int marker = 0; marker <= 4; ++marker) {
        const double seconds = timeOffset() + visibleDuration() * marker / 4.0;
        const int x = roundToInt(xForTime(seconds));
        graphics.drawText(String(seconds, 2) + "s",
                x - (marker == 4 ? 34 : 12), 91, 48, 16,
                Justification::centredLeft);
    }
    paintNoteGrid(graphics);
    paintNotes(graphics);
    paintVelocity(graphics);
    paintModulation(graphics);
    paintPlaybackCursor(graphics);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(11.f));
    graphics.drawText("Drag velocity bars (Shift: fine)  |  Right-click/Delete: remove  |  Scroll or overview: pan  |  Space: play",
            62, getHeight() - 23, getWidth() - 82, 17,
            Justification::centredLeft);
}

void PresetMidiEditor::paintMiniMap(Graphics& graphics) const {
    const auto map = miniMap();
    const auto pitches = overviewPitches();
    graphics.setColour(CanvasChromePalette::minimapBackground);
    graphics.fillRoundedRectangle(map, 3.f);
    Graphics::ScopedSaveState saveState(graphics);
    graphics.reduceClipRegion(map.getSmallestIntegerContainer());
    for (const auto& note : sequence.notes) {
        const float x = map.getX() + map.getWidth()
                * (float) (note.startSeconds / sequence.durationSeconds);
        const float width = jmax(2.f, map.getWidth()
                * (float) (note.durationSeconds / sequence.durationSeconds));
        const float y = map.getBottom() - map.getHeight()
                * ((float) (note.pitch - pitches.getStart()) + 0.5f)
                / (float) pitches.getLength();
        graphics.setColour(CanvasChromePalette::focus.withAlpha(0.76f));
        graphics.fillRoundedRectangle({ x, y - 1.f, width, 2.f }, 1.f);
    }
    const float left = map.getX() + map.getWidth()
            * (float) (timeOffset() / sequence.durationSeconds);
    const float width = map.getWidth()
            * (float) (visibleDuration() / sequence.durationSeconds);
    const float top = map.getBottom() - map.getHeight()
            * (float) (lowestPitch + 24 - pitches.getStart())
            / (float) pitches.getLength();
    const float height = map.getHeight() * 24.f / (float) pitches.getLength();
    const Rectangle<float> viewport(left, top, width, height);
    graphics.setColour(CanvasChromePalette::minimapViewport.withAlpha(0.12f));
    graphics.fillRoundedRectangle(viewport, 2.f);
    graphics.setColour(CanvasChromePalette::minimapViewport.withAlpha(0.85f));
    graphics.drawRoundedRectangle(viewport, 2.f, 1.2f);
}

void PresetMidiEditor::paintNoteGrid(Graphics& graphics) const {
    const auto grid = noteGrid();
    const float rowHeight = grid.getHeight() / 24.f;
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillRect(grid);
    for (int row = 0; row < 24; ++row) {
        const int pitch = lowestPitch + 23 - row;
        const float y = grid.getY() + row * rowHeight;
        graphics.setColour(isBlackKey(pitch)
                ? CanvasChromePalette::canvasBackground.withAlpha(0.55f)
                : Colours::white.withAlpha(0.035f));
        graphics.fillRect(Rectangle<float>(grid.getX(), y, grid.getWidth(), rowHeight));
        graphics.setColour(isBlackKey(pitch)
                ? CanvasChromePalette::restingControlSurface
                : CanvasChromePalette::text.withAlpha(0.62f));
        graphics.fillRect(Rectangle<float>(42.f, y + 0.5f, 18.f, rowHeight - 1.f));
        if (pitch % 12 == 0) {
            graphics.setColour(CanvasChromePalette::text);
            graphics.setFont(FontOptions(10.f));
            graphics.drawText(MidiMessage::getMidiNoteName(pitch, true, true, 3),
                    4, roundToInt(y), 36, roundToInt(rowHeight),
                    Justification::centredRight);
        }
        graphics.setColour(Colours::white.withAlpha(0.08f));
        graphics.drawHorizontalLine(roundToInt(y), grid.getX(), grid.getRight());
    }
    for (int step = 0; step <= 16; ++step) {
        const float x = xForTime(timeOffset() + step * gridStep());
        graphics.setColour(Colours::white.withAlpha(step % 4 == 0 ? 0.24f : 0.09f));
        graphics.drawVerticalLine(roundToInt(x), grid.getY(), grid.getBottom());
    }
}

void PresetMidiEditor::paintNotes(Graphics& graphics) const {
    const auto grid = noteGrid();
    const float rowHeight = grid.getHeight() / 24.f;
    Graphics::ScopedSaveState saveState(graphics);
    graphics.reduceClipRegion(grid.getSmallestIntegerContainer());
    for (size_t index = 0; index < sequence.notes.size(); ++index) {
        const auto& note = sequence.notes[index];
        const float x = xForTime(note.startSeconds);
        const float right = xForTime(note.startSeconds + note.durationSeconds);
        const float y = grid.getY() + rowHeight
                * (float) (lowestPitch + 23 - note.pitch);
        if (y < grid.getY() || y >= grid.getBottom()
                || right < grid.getX() || x > grid.getRight()) {
            continue;
        }
        const Rectangle<float> bar(x, y + 1.5f,
                jmax(3.f, right - x), rowHeight - 3.f);
        graphics.setColour(CanvasChromePalette::focus.withAlpha(
                (int) index == selectedNote ? 1.f : 0.77f));
        graphics.fillRoundedRectangle(bar, 2.f);
        if ((int) index == selectedNote) {
            graphics.setColour(Colours::white.withAlpha(0.9f));
            graphics.drawRoundedRectangle(bar, 2.f, 1.f);
            if (right >= grid.getX() && right <= grid.getRight()) {
                graphics.setColour(CanvasChromePalette::canvasBackground.withAlpha(0.9f));
                graphics.drawVerticalLine(roundToInt(right - 5.f),
                        y + 3.f, y + rowHeight - 3.f);
                graphics.drawVerticalLine(roundToInt(right - 3.f),
                        y + 3.f, y + rowHeight - 3.f);
            }
        }
        if (x < grid.getX()) {
            graphics.setColour(Colours::white.withAlpha(0.85f));
            graphics.drawLine(grid.getX() + 3.f, y + rowHeight * 0.5f,
                    grid.getX() + 8.f, y + rowHeight * 0.5f, 1.f);
        }
    }
}

void PresetMidiEditor::paintVelocity(Graphics& graphics) const {
    const auto grid = velocityGrid();
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillRect(grid);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(10.f, Font::bold));
    graphics.drawText("VEL", 5, roundToInt(grid.getY()), 35, 19,
            Justification::centredRight);
    for (int value : { 32, 64, 96 }) {
        const float y = grid.getBottom() - grid.getHeight() * value / 127.f;
        graphics.setColour(Colours::white.withAlpha(value == 64 ? 0.16f : 0.08f));
        graphics.drawHorizontalLine(roundToInt(y), grid.getX(), grid.getRight());
    }
    Graphics::ScopedSaveState saveState(graphics);
    graphics.reduceClipRegion(grid.getSmallestIntegerContainer());
    for (size_t index = 0; index < sequence.notes.size(); ++index) {
        const auto& note = sequence.notes[index];
        const float x = xForTime(note.startSeconds);
        const float height = grid.getHeight() * note.velocity / 127.f;
        graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(
                (int) index == selectedNote ? 0.96f : 0.54f));
        graphics.fillRoundedRectangle({ x - 4.f, grid.getBottom() - height,
                8.f, height }, 2.f);
        if ((int) index == selectedNote) {
            graphics.setColour(Colours::white);
            graphics.fillEllipse(x - 5.f, grid.getBottom() - height - 5.f,
                    10.f, 10.f);
        }
    }
}

void PresetMidiEditor::paintModulation(Graphics& graphics) const {
    const auto grid = controlGrid();
    const int lane = controllerLane();
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillRect(grid);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(10.f, Font::bold));
    graphics.drawText(lane == 1 ? "MOD" : "CC" + String(lane),
            3, roundToInt(grid.getY()), 38, 18,
            Justification::centredRight);
    for (int value : { 0, 32, 64, 96, 127 }) {
        const float y = grid.getBottom() - grid.getHeight() * value / 127.f;
        graphics.setColour(Colours::white.withAlpha(value == 64 ? 0.18f : 0.08f));
        graphics.drawHorizontalLine(roundToInt(y), grid.getX(), grid.getRight());
    }
    std::vector<size_t> points;
    for (size_t index = 0; index < sequence.controls.size(); ++index) {
        if (sequence.controls[index].controller == lane) {
            points.push_back(index);
        }
    }
    std::stable_sort(points.begin(), points.end(),
            [this](size_t first, size_t second) {
                return sequence.controls[first].timeSeconds
                        < sequence.controls[second].timeSeconds;
            });
    Graphics::ScopedSaveState saveState(graphics);
    graphics.reduceClipRegion(grid.getSmallestIntegerContainer());
    Path line;
    for (size_t position = 0; position < points.size(); ++position) {
        const auto& control = sequence.controls[points[position]];
        const float x = xForTime(control.timeSeconds);
        const float y = grid.getBottom() - grid.getHeight()
                * control.value / 127.f;
        if (position == 0) {
            line.startNewSubPath(x, y);
        } else {
            line.lineTo(x, y);
        }
    }
    if (points.size() >= 2) {
        Path fill(line);
        fill.lineTo(xForTime(sequence.controls[points.back()].timeSeconds),
                grid.getBottom());
        fill.lineTo(xForTime(sequence.controls[points.front()].timeSeconds),
                grid.getBottom());
        fill.closeSubPath();
        graphics.setColour(CanvasChromePalette::focus.withAlpha(0.12f));
        graphics.fillPath(fill);
    }
    graphics.setColour(CanvasChromePalette::focus.withAlpha(0.9f));
    graphics.strokePath(line, PathStrokeType(2.f));
    for (const size_t index : points) {
        const auto& control = sequence.controls[index];
        const float x = xForTime(control.timeSeconds);
        const float y = grid.getBottom() - grid.getHeight()
                * control.value / 127.f;
        graphics.setColour((int) index == selectedControl
                ? Colours::white : CanvasChromePalette::focus);
        graphics.fillEllipse(x - 4.f, y - 4.f, 8.f, 8.f);
        graphics.setColour(CanvasChromePalette::dockSurface);
        graphics.drawEllipse(x - 4.f, y - 4.f, 8.f, 8.f, 1.f);
    }
}

void PresetMidiEditor::paintPlaybackCursor(Graphics& graphics) const {
    if (!playbackTime) {
        return;
    }
    const auto position = playbackTime();
    if (!position.has_value()) {
        return;
    }
    const float x = xForTime(*position);
    if (x >= noteGrid().getX() && x <= noteGrid().getRight()) {
        graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(0.92f));
        graphics.drawVerticalLine(roundToInt(x),
                noteGrid().getY(), controlGrid().getBottom());
        Path marker;
        marker.addTriangle(x - 5.f, 108.f, x + 5.f, 108.f, x, 113.f);
        graphics.fillPath(marker);
    }
    const float mapX = miniMap().getX() + miniMap().getWidth()
            * (float) (*position / sequence.durationSeconds);
    graphics.setColour(CanvasChromePalette::navigationAccent);
    graphics.drawVerticalLine(roundToInt(mapX),
            miniMap().getY(), miniMap().getBottom());
}

}
