#include <algorithm>
#include <cmath>

#include "UI/PresetMidiEditor.h"
#include "UI/CanvasChromePalette.h"
#include "UI/MidiPatternMiniMap.h"

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

void PresetMidiEditor::SnapToggle::paintButton(Graphics& graphics, bool highlighted, bool down) {
    const auto area = getLocalBounds().toFloat().reduced(0.75f);
    const bool enabled = getToggleState();
    const auto accent = CanvasChromePalette::focus;
    graphics.setColour(enabled ? accent.withAlpha(down ? 0.3f : 0.16f)
            : CanvasChromePalette::restingControlSurface.brighter(highlighted ? 0.08f : 0.f));
    graphics.fillRoundedRectangle(area, area.getHeight() * 0.5f);
    graphics.setColour(enabled ? accent : CanvasChromePalette::mutedText);
    graphics.drawRoundedRectangle(area, area.getHeight() * 0.5f, 1.f);
    graphics.setFont(FontOptions(11.f, Font::bold));
    graphics.drawText(enabled ? "SNAP  ON" : "SNAP  OFF", getLocalBounds(), Justification::centred);
}

Rectangle<float> PresetMidiEditor::miniMap() const {
    return { 62.f, 40.f, (float) getWidth() - 80.f, 42.f };
}

Rectangle<float> PresetMidiEditor::playbackRuler() const {
    return { noteGrid().getX(), 88.f, noteGrid().getWidth(), 24.f };
}

Rectangle<float> PresetMidiEditor::noteGrid() const {
    constexpr float top = 112.f;
    const float reservedBelow = 25.f + 70.f + 36.f
            + 96.f * (float) controllers.size()
            + 8.f * (float) (controllers.size() - 1);
    return { 62.f, top, (float) getWidth() - 80.f,
            jmax(180.f, (float) getHeight() - top - reservedBelow) };
}

Rectangle<float> PresetMidiEditor::velocityGrid() const {
    return { noteGrid().getX(), noteGrid().getBottom() + 16.f,
            noteGrid().getWidth(), 70.f };
}

Rectangle<float> PresetMidiEditor::controlGrid() const {
    return controlGrid(controllers.front());
}

Rectangle<float> PresetMidiEditor::controlGrid(int controller) const {
    const auto lane = std::find(controllers.begin(), controllers.end(), controller);
    if (lane == controllers.end()) {
        return {};
    }
    const float offset = 104.f * (float) std::distance(controllers.begin(), lane);
    return { noteGrid().getX(), velocityGrid().getBottom() + 18.f + offset,
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
    return gridStep() * 16.0;
}

double PresetMidiEditor::gridStep() const {
    return 60.0 / sequence.tempoBpm / 4.0;
}

double PresetMidiEditor::timeOffset() const {
    return (double) timeOffsetSteps * gridStep();
}

int PresetMidiEditor::maximumTimeOffsetSteps() const {
    const double end = jmin(PresetMidiSequence::maximumDurationSeconds,
            jmax(sequence.durationSeconds, contentEndSeconds + visibleDuration() * 2.0));
    return timeOffsetLimit(end);
}

int PresetMidiEditor::timeOffsetLimit(double endSeconds) const {
    return jmax(0, (int) std::ceil(
            (endSeconds - visibleDuration()) / gridStep()));
}

double PresetMidiEditor::timeAt(float x) const {
    const double time = rawTimeAt(x);
    return snapButton.getToggleState()
            ? jlimit(0.0, PresetMidiSequence::maximumDurationSeconds,
                    std::round(time / gridStep()) * gridStep()) : time;
}

double PresetMidiEditor::rawTimeAt(float x) const {
    const auto grid = noteGrid();
    const float proportion = (x - grid.getX()) / grid.getWidth();
    return jlimit(0.0, PresetMidiSequence::maximumDurationSeconds,
            timeOffset() + proportion * visibleDuration());
}

float PresetMidiEditor::xForTime(double time) const {
    return noteGrid().getX() + noteGrid().getWidth()
            * (float) ((time - timeOffset()) / visibleDuration());
}

Range<int> PresetMidiEditor::overviewPitches() const {
    return MidiPatternMiniMap::pitchRange(sequence, 48, 12);
}

void PresetMidiEditor::paint(Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::dockSurface);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(FontOptions(15.f, Font::bold));
    graphics.drawText("PRESET PHRASE", 16, 8, 150, 24,
            Justification::centredLeft);
    if (activeNote >= 0) {
        graphics.setColour(CanvasChromePalette::focus);
        graphics.setFont(FontOptions(12.f, Font::bold));
        graphics.drawText(resizingNote ? "RESIZE NOTE" : "MOVE NOTE",
                324, 9, jmax(0, snapButton.getX() - 336), 22, Justification::centredLeft);
    } else if (activeVelocity >= 0) {
        graphics.setColour(CanvasChromePalette::focus);
        graphics.setFont(FontOptions(12.f, Font::bold));
        graphics.drawText("VELOCITY "
                        + String(sequence.notes[(size_t) activeVelocity].velocity),
                324, 9, jmax(0, snapButton.getX() - 336), 22, Justification::centredLeft);
    } else if (selectedNote >= 0 && selectedNote < (int) sequence.notes.size()) {
        const auto& note = sequence.notes[(size_t) selectedNote];
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.setFont(FontOptions(11.f));
        graphics.drawText(MidiMessage::getMidiNoteName(note.pitch, true, true, 3)
                        + "  V" + String(note.velocity)
                        + "  " + String(note.startSeconds, 2)
                        + "s  LEN " + String(note.durationSeconds, 2) + "s",
                324, 9, jmax(0, snapButton.getX() - 336), 22, Justification::centredLeft);
    }
    paintMiniMap(graphics);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(10.f));
    for (int step = 0; step <= 16; ++step) {
        const int absoluteStep = timeOffsetSteps + step;
        if (absoluteStep % 4 != 0) {
            continue;
        }
        const int beat = absoluteStep / 4;
        const int x = roundToInt(xForTime(absoluteStep * gridStep()));
        graphics.drawText(String(beat / 4 + 1) + "." + String(beat % 4 + 1),
                x - (step == 16 ? 30 : 8), 91, 40, 16, Justification::centredLeft);
    }
    paintNoteGrid(graphics);
    paintNotes(graphics);
    paintVelocity(graphics);
    paintModulation(graphics);
    paintPlaybackCursor(graphics);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(11.f));
    graphics.drawText("Velocity: drag (Shift: fine)  |  Delete: remove  |  Scroll: pan  |  Ruler/overview: seek  |  Shift-Up/Down: transpose  |  Space: play",
            62, getHeight() - 23, getWidth() - 82, 17,
            Justification::centredLeft);
}

void PresetMidiEditor::paintMiniMap(Graphics& graphics) const {
    const auto map = miniMap();
    const auto pitches = overviewPitches();
    graphics.setColour(CanvasChromePalette::minimapBackground);
    graphics.fillRoundedRectangle(map, 3.f);
    MidiPatternMiniMap::paintNotes(graphics, sequence,
            map.withTrimmedBottom(9.f), pitches);
    MidiPatternMiniMap::paintControls(graphics, sequence,
            map.withTrimmedTop(map.getHeight() - 9.f), controllers.front());
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
        graphics.setColour(Colours::white.withAlpha(
                (timeOffsetSteps + step) % 16 == 0 ? 0.32f
                        : ((timeOffsetSteps + step) % 4 == 0 ? 0.24f : 0.09f)));
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
        const float y = grid.getBottom() - grid.getHeight()
                * static_cast<float>(value) / 127.f;
        graphics.setColour(Colours::white.withAlpha(value == 64 ? 0.16f : 0.08f));
        graphics.drawHorizontalLine(roundToInt(y), grid.getX(), grid.getRight());
    }
    Graphics::ScopedSaveState saveState(graphics);
    graphics.reduceClipRegion(grid.getSmallestIntegerContainer());
    for (size_t index = 0; index < sequence.notes.size(); ++index) {
        const auto& note = sequence.notes[index];
        const float x = xForTime(note.startSeconds);
        const float height = grid.getHeight() * note.velocity / 127.f;
        graphics.setColour(CanvasChromePalette::focus.withAlpha(
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
    for (const int controller : controllers) {
        paintModulationLane(graphics, controller);
    }
}

void PresetMidiEditor::paintModulationLane(Graphics& graphics, int controller) const {
    const auto grid = controlGrid(controller);
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillRect(grid);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(10.f, Font::bold));
    graphics.drawText(controller == 1 ? "MOD" : "CC " + String(controller),
            2, roundToInt(grid.getY()) + 5, 56, 20,
            Justification::centredRight);
    for (int value : { 0, 32, 64, 96, 127 }) {
        const float y = grid.getBottom() - grid.getHeight()
                * static_cast<float>(value) / 127.f;
        graphics.setColour(Colours::white.withAlpha(value == 64 ? 0.18f : 0.08f));
        graphics.drawHorizontalLine(roundToInt(y), grid.getX(), grid.getRight());
    }
    std::vector<size_t> points;
    for (size_t index = 0; index < sequence.controls.size(); ++index) {
        if (sequence.controls[index].controller == controller) {
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
                * static_cast<float>(control.value) / 127.f;
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
        graphics.setColour(CanvasChromePalette::patternAutomation.withAlpha(0.12f));
        graphics.fillPath(fill);
    }
    graphics.setColour(CanvasChromePalette::patternAutomation.withAlpha(0.9f));
    graphics.strokePath(line, PathStrokeType(2.f));
    for (const size_t index : points) {
        const auto& control = sequence.controls[index];
        const float x = xForTime(control.timeSeconds);
        const float y = grid.getBottom() - grid.getHeight()
                * static_cast<float>(control.value) / 127.f;
        graphics.setColour((int) index == selectedControl
                ? Colours::white : CanvasChromePalette::patternAutomation);
        graphics.fillEllipse(x - 4.f, y - 4.f, 8.f, 8.f);
        graphics.setColour(CanvasChromePalette::dockSurface);
        graphics.drawEllipse(x - 4.f, y - 4.f, 8.f, 8.f, 1.f);
    }
}

void PresetMidiEditor::paintPlaybackCursor(Graphics& graphics) const {
    const double position = cursorPosition();
    const float x = xForTime(position);
    if (x >= noteGrid().getX() && x <= noteGrid().getRight()) {
        graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(0.92f));
        graphics.drawVerticalLine(roundToInt(x),
                noteGrid().getY(), controlGrid(controllers.back()).getBottom());
        Path marker;
        marker.addTriangle(x - 5.f, 108.f, x + 5.f, 108.f, x, 113.f);
        graphics.fillPath(marker);
    }
    const float mapX = miniMap().getX() + miniMap().getWidth()
            * (float) (position / sequence.durationSeconds);
    graphics.setColour(CanvasChromePalette::navigationAccent);
    graphics.drawVerticalLine(roundToInt(mapX),
            miniMap().getY(), miniMap().getBottom());
}

}
