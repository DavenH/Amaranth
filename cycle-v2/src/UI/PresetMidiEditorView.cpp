#include <algorithm>
#include <cmath>

#include "UI/PresetMidiEditor.h"
#include "UI/CanvasChromePalette.h"
#include "UI/MidiPatternMiniMap.h"

namespace CycleV2 {

using namespace juce;

void PresetMidiEditor::SnapToggle::paintButton(Graphics& graphics, bool highlighted, bool down) {
    const bool enabled = getToggleState();
    const Rectangle<float> track(2.f, 7.f, 28.f, 14.f);
    graphics.setColour(enabled ? CanvasChromePalette::focus.withAlpha(down ? 0.7f : 0.9f)
            : CanvasChromePalette::border.brighter(highlighted ? 0.15f : 0.f));
    graphics.fillRoundedRectangle(track, 7.f);
    graphics.setColour(CanvasChromePalette::text);
    graphics.fillEllipse(enabled ? 18.f : 4.f, 9.f, 10.f, 10.f);
    graphics.setFont(FontOptions(12.f));
    graphics.drawText("Snap 1/16", 38, 0, getWidth() - 38, getHeight(),
            Justification::centredLeft);
}

Rectangle<float> PresetMidiEditor::miniMap() const {
    return { 80.f, 44.f, (float) getWidth() - 96.f, 32.f };
}

Rectangle<float> PresetMidiEditor::playbackRuler() const {
    return { noteGrid().getX(), miniMap().getBottom(), noteGrid().getWidth(), 24.f };
}

Rectangle<float> PresetMidiEditor::noteGrid() const {
    constexpr float top = 100.f;
    const float bodyHeight = jmax(240.f, (float) getHeight() - top - velocityHeight - 3 * laneGap);
    const float laneHeight = (float) controllerStride * (float) controllers.size() - laneGap;
    const float automationHeight = jmin(jmax((float) controllerHeight, laneHeight), bodyHeight * 0.4f);
    return { 80.f, top, (float) getWidth() - 96.f,
            jmax(180.f, bodyHeight - automationHeight) };
}

Rectangle<float> PresetMidiEditor::pitchRow(int pitch) const {
    const auto grid = noteGrid();
    const float height = grid.getHeight() / (float) visiblePitches;
    return { grid.getX(), grid.getY() + height * (float) (lowestPitch + visiblePitches - 1 - pitch),
            grid.getWidth(), height };
}

Rectangle<float> PresetMidiEditor::velocityGrid() const {
    return { noteGrid().getX(), noteGrid().getBottom() + laneGap,
            noteGrid().getWidth(), (float) velocityHeight };
}

Rectangle<float> PresetMidiEditor::automationBounds() const {
    const float top = velocityGrid().getBottom() + laneGap;
    return { noteGrid().getX(), top, noteGrid().getWidth(),
            jmax(0.f, (float) getHeight() - laneGap - top) };
}

Rectangle<float> PresetMidiEditor::controlGrid() const {
    return controlGrid(controllers.front());
}

Rectangle<float> PresetMidiEditor::controlGrid(int controller) const {
    const auto lane = std::find(controllers.begin(), controllers.end(), controller);
    if (lane == controllers.end()) {
        return {};
    }
    const float offset = (float) controllerStride * (float) std::distance(controllers.begin(), lane)
            - (float) automationScroll.getCurrentRangeStart();
    return { noteGrid().getX(), automationBounds().getY() + offset,
            noteGrid().getWidth(), (float) controllerHeight };
}

int PresetMidiEditor::pitchAt(float y) const {
    const auto grid = noteGrid();
    const int row = jlimit(0, visiblePitches - 1,
            (int) ((y - grid.getY()) * (float) visiblePitches / grid.getHeight()));
    return lowestPitch + visiblePitches - 1 - row;
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
    paintMiniMap(graphics);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(10.f));
    const auto ruler = playbackRuler();
    for (int step = 0; step <= 16; ++step) {
        const int absoluteStep = timeOffsetSteps + step;
        const int x = roundToInt(xForTime(absoluteStep * gridStep()));
        const bool beatTick = absoluteStep % 4 == 0;
        graphics.setColour(CanvasChromePalette::mutedText.withAlpha(beatTick ? 0.9f : 0.55f));
        graphics.drawVerticalLine(x, ruler.getBottom() - (beatTick ? 7.f : 4.f),
                ruler.getBottom());
        if (beatTick) {
            const int beat = absoluteStep / 4;
            graphics.drawText(String(beat / 4 + 1) + "." + String(beat % 4 + 1),
                    x - (step == 16 ? 25 : 8), roundToInt(ruler.getY()), 32, 17,
                    Justification::centredLeft);
        }
    }
    paintNoteGrid(graphics);
    paintNotes(graphics);
    paintVelocity(graphics);
    paintModulation(graphics);
    paintPlaybackCursor(graphics);
}

void PresetMidiEditor::paintMiniMap(Graphics& graphics) const {
    const auto map = miniMap();
    const auto pitches = overviewPitches();
    graphics.setColour(CanvasChromePalette::raisedSurface.withAlpha(0.55f));
    graphics.fillRect(map);
    MidiPatternMiniMap::paintNotes(graphics, sequence, map.reduced(0.f, 4.f), pitches);
}

void PresetMidiEditor::paintNoteGrid(Graphics& graphics) const {
    const auto grid = noteGrid();
    const auto keyboard = grid.withX(28.f).withWidth(50.f);
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.62f));
    graphics.fillRect(keyboard);
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillRect(grid);
    for (int row = 0; row < visiblePitches; ++row) {
        const int pitch = lowestPitch + visiblePitches - 1 - row;
        const auto area = pitchRow(pitch);
        const float y = area.getY();
        const bool black = MidiMessage::isMidiNoteBlack(pitch);
        graphics.setColour(black ? CanvasChromePalette::canvasBackground.withAlpha(0.55f)
                : Colours::white.withAlpha(0.025f));
        graphics.fillRect(area);
        graphics.setColour(CanvasChromePalette::canvasBackground);
        if (black) {
            graphics.fillRect(Rectangle<float>(keyboard.getX(), y + 0.5f,
                    32.f, area.getHeight() - 1.f));
            graphics.drawHorizontalLine(roundToInt(area.getCentreY()),
                    keyboard.getX() + 32.f, keyboard.getRight());
        } else if (!MidiMessage::isMidiNoteBlack(pitch + 1)) {
            graphics.drawHorizontalLine(roundToInt(y), keyboard.getX(), keyboard.getRight());
        }
        if (pitch % 12 == 0) {
            graphics.setColour(CanvasChromePalette::text);
            graphics.setFont(FontOptions(10.f));
            graphics.drawText(MidiMessage::getMidiNoteName(pitch, true, true, 3),
                    0, roundToInt(y), 25, roundToInt(area.getHeight()),
                    Justification::centredRight);
        }
        graphics.setColour(Colours::white.withAlpha(0.035f));
        graphics.drawHorizontalLine(roundToInt(y), grid.getX(), grid.getRight());
    }
    for (int step = 0; step <= 16; ++step) {
        const float x = xForTime(timeOffset() + step * gridStep());
        graphics.setColour(Colours::white.withAlpha(
                (timeOffsetSteps + step) % 16 == 0 ? 0.3f
                        : ((timeOffsetSteps + step) % 4 == 0 ? 0.2f : 0.055f)));
        graphics.drawVerticalLine(roundToInt(x), grid.getY(), grid.getBottom());
    }
}

void PresetMidiEditor::paintNotes(Graphics& graphics) const {
    const auto grid = noteGrid();
    const float rowHeight = grid.getHeight() / (float) visiblePitches;
    Graphics::ScopedSaveState saveState(graphics);
    graphics.reduceClipRegion(grid.getSmallestIntegerContainer());
    for (size_t index = 0; index < sequence.notes.size(); ++index) {
        const auto& note = sequence.notes[index];
        const float x = xForTime(note.startSeconds);
        const float right = xForTime(note.startSeconds + note.durationSeconds);
        const float y = pitchRow(note.pitch).getY();
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
    paintLaneGrid(graphics, grid);
    Graphics::ScopedSaveState saveState(graphics);
    graphics.reduceClipRegion(grid.getSmallestIntegerContainer());
    for (size_t index = 0; index < sequence.notes.size(); ++index) {
        const auto& note = sequence.notes[index];
        const float x = xForTime(note.startSeconds);
        const float height = grid.getHeight() * note.velocity / 127.f;
        graphics.setColour(CanvasChromePalette::focus.withAlpha(
                (int) index == selectedNote ? 0.96f : 0.54f));
        graphics.fillRect(Rectangle<float>(x - 1.f, grid.getBottom() - height, 2.f, height));
        graphics.setColour((int) index == selectedNote ? CanvasChromePalette::text
                : CanvasChromePalette::focus);
        graphics.fillRect(Rectangle<float>(x - 2.5f, grid.getBottom() - height, 5.f, 2.f));
    }
    paintLaneLabel(graphics, grid, "Velocity");
}

void PresetMidiEditor::paintLaneGrid(Graphics& graphics, Rectangle<float> grid) const {
    graphics.setColour(CanvasChromePalette::raisedSurface.withAlpha(0.55f));
    graphics.fillRect(grid);
    graphics.setFont(FontOptions(10.f));
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.drawText("127", 52, roundToInt(grid.getY()), 24, 12, Justification::centredRight);
    graphics.drawText("0", 52, roundToInt(grid.getBottom()) - 12, 24, 12,
            Justification::centredRight);
    for (const int value : { 0, 64, 127 }) {
        const float y = grid.getBottom() - grid.getHeight() * (float) value / 127.f;
        graphics.setColour(Colours::white.withAlpha(0.13f));
        graphics.drawHorizontalLine(roundToInt(y), grid.getX(), grid.getRight());
    }
    for (int step = 0; step <= 16; ++step) {
        if ((timeOffsetSteps + step) % 4 == 0) {
            graphics.setColour(Colours::white.withAlpha(0.15f));
            graphics.drawVerticalLine(roundToInt(xForTime(timeOffset() + step * gridStep())),
                    grid.getY(), grid.getBottom());
        }
    }
}

void PresetMidiEditor::paintLaneLabel(Graphics& graphics, Rectangle<float> grid,
        const String& label) const {
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.85f));
    graphics.setFont(FontOptions(10.f));
    graphics.drawText(label, grid.reduced(8.f, 4.f).removeFromTop(16.f),
            Justification::topRight);
}

void PresetMidiEditor::paintModulation(Graphics& graphics) const {
    Graphics::ScopedSaveState saveState(graphics);
    const auto area = automationBounds();
    graphics.reduceClipRegion(area.withX(0.f).withRight((float) getWidth()).getSmallestIntegerContainer());
    for (const int controller : controllers) {
        paintModulationLane(graphics, controller);
    }
}

void PresetMidiEditor::paintModulationLane(Graphics& graphics, int controller) const {
    const auto grid = controlGrid(controller);
    if (!grid.intersects(automationBounds())) {
        return;
    }
    paintLaneGrid(graphics, grid);
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
        graphics.setColour(CanvasChromePalette::patternAutomation.withAlpha(0.045f));
        graphics.fillPath(fill);
    }
    graphics.setColour(CanvasChromePalette::patternAutomation.withAlpha(0.9f));
    graphics.strokePath(line, PathStrokeType(1.3f));
    for (const size_t index : points) {
        const auto& control = sequence.controls[index];
        const float x = xForTime(control.timeSeconds);
        const float y = grid.getBottom() - grid.getHeight()
                * static_cast<float>(control.value) / 127.f;
        graphics.setColour((int) index == selectedControl
                ? Colours::white : CanvasChromePalette::patternAutomation);
        graphics.fillEllipse(x - 2.5f, y - 2.5f, 5.f, 5.f);
        graphics.setColour(CanvasChromePalette::dockSurface);
        graphics.drawEllipse(x - 2.5f, y - 2.5f, 5.f, 5.f, 0.75f);
    }
    paintLaneLabel(graphics, grid, controller == 1 ? "MOD" : "CC " + String(controller));
}

void PresetMidiEditor::paintPlaybackCursor(Graphics& graphics) const {
    const double position = cursorPosition();
    const float x = xForTime(position);
    if (x >= noteGrid().getX() && x <= noteGrid().getRight()) {
        graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(0.92f));
        graphics.drawVerticalLine(roundToInt(x),
                noteGrid().getY(), automationBounds().getBottom());
        Path marker;
        marker.addTriangle(x - 4.f, noteGrid().getY() - 5.f,
                x + 4.f, noteGrid().getY() - 5.f, x, noteGrid().getY());
        graphics.fillPath(marker);
    }
    const float mapX = miniMap().getX() + miniMap().getWidth()
            * (float) (position / sequence.durationSeconds);
    graphics.setColour(CanvasChromePalette::navigationAccent);
    graphics.drawVerticalLine(roundToInt(mapX),
            miniMap().getY(), miniMap().getBottom());
}

}
