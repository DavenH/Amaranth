#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphSerializer.h"
#include "Graph/PresetMidiControllerMapping.h"
#include "UI/PresetMidiEditor.h"

using namespace CycleV2;
using namespace juce;

namespace {

MouseEvent pointer(Component& component, Point<float> position,
        ModifierKeys modifiers = ModifierKeys::leftButtonModifier) {
    const Time now = Time::getCurrentTime();
    return {
            Desktop::getInstance().getMainMouseSource(), position, modifiers,
            1.f, 0.f, 0.f, 0.f, 0.f, &component, &component,
            now, position, now, 1, false
    };
}

TEST_CASE("Alto sax preset exposes its attached CC mappings",
        "[cycle-v2][preset][sequence][editor]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content/presets/altosax-2.cyclegraph");
    REQUIRE(preset.existsAsFile());
    const auto loaded = GraphSerializer().loadJsonString(preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    const std::vector<int> expected { 1, 73 };
    REQUIRE(mappedPresetMidiControllers(loaded.graph) == expected);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

float xAt(const PresetMidiEditor& editor, double time, double duration) {
    const auto grid = editor.noteGrid();
    return grid.getX() + grid.getWidth()
            * (float) (time / (duration * 0.5));
}

float pitch60Y(const PresetMidiEditor& editor) {
    return editor.pitchRow(60).getCentreY();
}

}

TEST_CASE("Piano roll auditions a note and publishes one move gesture",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes.push_back({ 60, 95, 1.0, 0.5 });
    PresetMidiSequence changed;
    int publications = 0;
    int noteOns = 0;
    int noteOffs = 0;
    int transportToggles = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence sequence) {
        changed = std::move(sequence);
        ++publications;
    }, [&] { ++transportToggles; }, [&](int pitch, int velocity, bool on) {
        REQUIRE(pitch == 60);
        if (on) {
            REQUIRE(velocity == 95);
            ++noteOns;
        } else {
            ++noteOffs;
        }
    });
    const float y = pitch60Y(editor);
    const float x = xAt(editor, 1.125, 4.0);
    const float stepWidth = editor.noteGrid().getWidth() / 16.f;
    editor.mouseDown(pointer(editor, { x, y }));
    REQUIRE(noteOns == 1);
    editor.mouseDrag(pointer(editor, { x + stepWidth * 2.f, y }));
    REQUIRE(publications == 0);
    editor.mouseUp(pointer(editor, { x + stepWidth * 2.f, y }));
    REQUIRE(noteOffs == 1);
    REQUIRE(publications == 1);
    REQUIRE(changed.notes[0].startSeconds == 1.25);
    REQUIRE(changed.notes[0].durationSeconds == 0.5);
    REQUIRE(editor.keyPressed(KeyPress(KeyPress::spaceKey)));
    REQUIRE(transportToggles == 1);
}

TEST_CASE("Velocity and modulation lanes edit selected saved events",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes.push_back({ 60, 80, 1.0, 0.5 });
    PresetMidiSequence changed;
    int publications = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence sequence) {
        changed = std::move(sequence);
        ++publications;
    });
    const auto velocity = editor.velocityGrid();
    const float x = xAt(editor, 1.0, 4.0);
    const float y80 = velocity.getBottom() - velocity.getHeight() * 80.f / 127.f;
    const float y100 = velocity.getBottom() - velocity.getHeight() * 100.f / 127.f;
    editor.mouseDown(pointer(editor, { x, y80 }));
    editor.mouseDrag(pointer(editor, { x, y100 }));
    editor.mouseUp(pointer(editor, { x, y100 }));
    REQUIRE(publications == 1);
    REQUIRE(changed.notes[0].velocity == 100);

    editor.mouseDown(pointer(editor, { x, y100 }));
    editor.mouseDrag(pointer(editor, { x, y100 - 20.f },
            ModifierKeys::leftButtonModifier | ModifierKeys::shiftModifier));
    editor.mouseUp(pointer(editor, { x, y100 - 20.f }));
    REQUIRE(changed.notes[0].velocity == 110);
    REQUIRE(editor.keyPressed(KeyPress(KeyPress::upKey)));
    REQUIRE(changed.notes[0].velocity == 111);

    const auto control = editor.controlGrid();
    const float controlX = xAt(editor, 0.5, 4.0);
    const float controlY = control.getBottom() - control.getHeight() * 64.f / 127.f;
    editor.mouseDown(pointer(editor, { controlX, controlY }));
    editor.mouseUp(pointer(editor, { controlX, controlY }));
    REQUIRE(changed.controls.size() == 1);
    editor.mouseDown(pointer(editor, { controlX, controlY },
            ModifierKeys::rightButtonModifier));
    REQUIRE(changed.controls.empty());
}

TEST_CASE("Piano roll keeps mapped CC automation in separate stacked lanes",
        "[cycle-v2][preset][sequence][editor][ui]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.controls.push_back({ 1, 20, 0.5 });
    phrase.controls.push_back({ 73, 60, 0.5 });
    PresetMidiSequence changed;
    int publications = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence sequence) {
        changed = std::move(sequence);
        ++publications;
    }, {}, {}, {}, { 1, 73 });
    REQUIRE(editor.controllerLanes() == std::vector<int>({ 1, 73 }));
    REQUIRE(editor.controlGrid(73).getY() > editor.controlGrid(1).getBottom());
    REQUIRE(editor.controlGrid(73).getBottom() < editor.getHeight());

    const auto lane = editor.controlGrid(73);
    const float x = xAt(editor, 1.0, 4.0);
    const float y = lane.getBottom() - lane.getHeight() * 100.f / 127.f;
    editor.mouseDown(pointer(editor, { x, y }));
    editor.mouseUp(pointer(editor, { x, y }));
    REQUIRE(publications == 1);
    REQUIRE(changed.controls.size() == 3);
    REQUIRE(changed.controls.back().controller == 73);
    REQUIRE(changed.controls[0].controller == 1);
    REQUIRE(changed.controls[0].value == 20);

    const float movedX = xAt(editor, 1.125, 4.0);
    const float movedY = lane.getBottom() - lane.getHeight() * 40.f / 127.f;
    editor.mouseDown(pointer(editor, { x, y }));
    editor.mouseDrag(pointer(editor, { movedX, movedY }));
    editor.mouseUp(pointer(editor, { movedX, movedY }));
    REQUIRE(publications == 2);
    REQUIRE(changed.controls[2].controller == 73);
    REQUIRE(changed.controls[2].value == 40);
    REQUIRE(changed.controls[0].value == 20);

    const auto firstLane = editor.controlGrid(1);
    const float firstY = firstLane.getBottom() - firstLane.getHeight() * 85.f / 127.f;
    const float laterX = xAt(editor, 1.5, 4.0);
    editor.mouseDown(pointer(editor, { laterX, firstY }));
    editor.mouseUp(pointer(editor, { laterX, firstY }));
    REQUIRE(publications == 3);
    REQUIRE(changed.controls.size() == 4);
    REQUIRE(changed.controls.back().controller == 1);
    REQUIRE(changed.controls[2].controller == 73);

    PresetMidiEditor afterMappingChange(changed, [](PresetMidiSequence) {},
            {}, {}, {}, { 1 });
    REQUIRE(afterMappingChange.controllerLanes() == std::vector<int>({ 1, 73 }));
}

TEST_CASE("Trackpad deltas accumulate and overview seeking preserves pitch navigation",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    PresetMidiSequence changed;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence sequence) {
        changed = std::move(sequence);
    });
    MouseWheelDetails wheel {};
    wheel.deltaX = -0.01f;
    wheel.deltaY = 0.01f;
    for (int index = 0; index < 7; ++index) {
        editor.mouseWheelMove(pointer(editor, { 200.f, 200.f }), wheel);
    }
    const auto grid = editor.noteGrid();
    editor.mouseDown(pointer(editor, { grid.getX() + 2.f, grid.getCentreY() }));
    editor.mouseUp(pointer(editor, { grid.getX() + 2.f, grid.getCentreY() }));
    REQUIRE(changed.notes.size() == 1);
    REQUIRE(changed.notes[0].startSeconds == 0.125);
    REQUIRE(changed.notes[0].pitch == 67);
    const auto control = editor.controlGrid();
    editor.mouseDown(pointer(editor, { grid.getX() + 2.f, control.getCentreY() }));
    editor.mouseUp(pointer(editor, { grid.getX() + 2.f, control.getCentreY() }));
    REQUIRE(changed.controls[0].timeSeconds == changed.notes[0].startSeconds);

    const auto map = editor.miniMap();
    editor.mouseDown(pointer(editor,
            { map.getX() + map.getWidth() * 0.9f,
                    map.getY() + map.getHeight() * 0.2f }));
    editor.mouseUp(pointer(editor,
            { map.getX() + map.getWidth() * 0.9f,
                    map.getY() + map.getHeight() * 0.2f }));
    editor.mouseDown(pointer(editor, { grid.getX() + 2.f, grid.getCentreY() }));
    editor.mouseUp(pointer(editor, { grid.getX() + 2.f, grid.getCentreY() }));
    REQUIRE(changed.notes.back().startSeconds == 2.0);
    REQUIRE(changed.notes.back().pitch == changed.notes.front().pitch);
}

TEST_CASE("Major piano-roll beat lines stay tied to absolute beats while panning",
        "[cycle-v2][preset][sequence][editor][ui]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    PresetMidiEditor editor(phrase, [](PresetMidiSequence) {});
    const auto grid = editor.noteGrid();
    const int y = roundToInt(grid.getY() + grid.getHeight() / 48.f);
    const auto lineX = [&](int step) {
        return roundToInt(grid.getX() + grid.getWidth() * step / 16.f);
    };
    const Image before = editor.createComponentSnapshot(editor.getLocalBounds());
    REQUIRE(before.getPixelAt(lineX(4), y).getBrightness()
            > before.getPixelAt(lineX(3), y).getBrightness());

    MouseWheelDetails wheel {};
    wheel.deltaX = -0.0625f;
    editor.mouseWheelMove(pointer(editor, { grid.getCentreX(), grid.getCentreY() }),
            wheel);
    const Image after = editor.createComponentSnapshot(editor.getLocalBounds());
    REQUIRE(after.getPixelAt(lineX(3), y).getBrightness()
            > after.getPixelAt(lineX(4), y).getBrightness());
}

TEST_CASE("A note with its start offscreen exposes a distinct resize gesture",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes.push_back({ 60, 90, 0.0, 1.5 });
    PresetMidiSequence changed;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence sequence) {
        changed = std::move(sequence);
    });
    MouseWheelDetails wheel {};
    wheel.deltaX = -0.5f;
    editor.mouseWheelMove(pointer(editor, { 200.f, 200.f }), wheel);
    const float right = editor.noteGrid().getX() + editor.noteGrid().getWidth() * 0.25f;
    const float y = pitch60Y(editor);
    const float twoSteps = editor.noteGrid().getWidth() / 8.f;
    editor.mouseDown(pointer(editor, { right - 5.f, y }));
    editor.mouseDrag(pointer(editor, { right - 5.f + twoSteps, y }));
    editor.mouseUp(pointer(editor, { right - 5.f + twoSteps, y }));
    REQUIRE(changed.notes[0].startSeconds == 0.0);
    REQUIRE(changed.notes[0].durationSeconds == 1.75);
}

TEST_CASE("The enlarged piano roll paints a shared playback cursor",
        "[cycle-v2][preset][sequence][editor][ui]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    std::optional<double> playbackTime = 1.0;
    PresetMidiEditor editor(phrase, [](PresetMidiSequence) {}, {}, {},
            [&] { return playbackTime; });
    REQUIRE(editor.getWidth() >= 800);
    REQUIRE(editor.controlGrid().getHeight() == 76.f);
    REQUIRE(editor.velocityGrid().getHeight() == 48.f);
    REQUIRE(editor.miniMap().getWidth() == editor.noteGrid().getWidth());
    const Image image = editor.createComponentSnapshot(editor.getLocalBounds());
    const int cursorX = roundToInt(xAt(editor, 1.0, 4.0));
    const Colour cursor = image.getPixelAt(cursorX,
            roundToInt(editor.noteGrid().getY() + 20.f));
    playbackTime.reset();
    const Image withoutCursor = editor.createComponentSnapshot(editor.getLocalBounds());
    REQUIRE(cursor.getBrightness() > withoutCursor.getPixelAt(cursorX,
            roundToInt(editor.noteGrid().getY() + 20.f)).getBrightness() + 0.3f);
    const auto grid = editor.noteGrid();
    const int stripeX = roundToInt(grid.getX() + 24.f);
    const Colour whiteRow = image.getPixelAt(stripeX,
            roundToInt(editor.pitchRow(60).getCentreY()));
    const Colour blackRow = image.getPixelAt(stripeX,
            roundToInt(editor.pitchRow(61).getCentreY()));
    REQUIRE(whiteRow.getBrightness() > blackRow.getBrightness());
}

TEST_CASE("Tempo entry preserves authored timing and saved trailing silence",
        "[cycle-v2][preset][sequence][editor][tempo]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 6.0;
    phrase.notes.push_back({ 60, 100, 0.0, 0.5 });
    phrase.controls.push_back({ 1, 64, 0.75 });
    PresetMidiSequence changed;
    int publications = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence sequence) {
        changed = std::move(sequence);
        ++publications;
    });
    Label* tempo = nullptr;
    for (auto* child : editor.getChildren()) {
        REQUIRE(child->getName() != "PresetMidiEditor.Length");
        if (child->getName() == "PresetMidiEditor.Tempo") {
            tempo = dynamic_cast<Label*>(child);
        }
    }
    REQUIRE(tempo != nullptr);
    editor.addToDesktop(ComponentPeer::windowIsTemporary);
    editor.setVisible(true);
    tempo->showEditor();
    auto* entry = tempo->getCurrentTextEditor();
    REQUIRE(entry != nullptr);
    entry->setText("97.3");
    tempo->hideEditor(false);
    REQUIRE(publications == 1);
    REQUIRE(changed.tempoBpm == 97.3);
    REQUIRE(tempo->getText() == "97.3 BPM");
    REQUIRE(changed.durationSeconds == 6.0);
    REQUIRE(changed.notes[0].durationSeconds == 0.5);
    REQUIRE(changed.controls[0].timeSeconds == 0.75);
    tempo->setText("invalid", sendNotificationSync);
    REQUIRE(publications == 1);
    REQUIRE(tempo->getText() == "97.3 BPM");
    tempo->setText("999", sendNotificationSync);
    REQUIRE(changed.tempoBpm == PresetMidiSequence::maximumTempoBpm);
    tempo->setText("1", sendNotificationSync);
    REQUIRE(changed.tempoBpm == PresetMidiSequence::minimumTempoBpm);
}

TEST_CASE("Shift arrows transpose the whole phrase without altering expression",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes = {{60, 80, 0.0, 0.5}, {64, 110, 1.0, 0.75}};
    phrase.controls = {{1, 73, 0.25}};
    PresetMidiSequence changed;
    int publications = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
        changed = std::move(next);
        ++publications;
    });
    REQUIRE(editor.keyPressed(KeyPress(KeyPress::upKey, ModifierKeys::shiftModifier, 0)));
    REQUIRE(publications == 1);
    REQUIRE(changed.notes[0].pitch == 61);
    REQUIRE(changed.notes[1].pitch == 65);
    REQUIRE(changed.notes[1].velocity == 110);
    REQUIRE(changed.notes[1].startSeconds == 1.0);
    REQUIRE(changed.notes[1].durationSeconds == 0.75);
    REQUIRE(changed.durationSeconds == 4.0);
    REQUIRE(changed.controls[0].value == 73);
    REQUIRE(changed.controls[0].timeSeconds == 0.25);
    editor.keyPressed(KeyPress(KeyPress::downKey, ModifierKeys::shiftModifier, 0));
    REQUIRE(publications == 2);
    REQUIRE(changed.notes[0].pitch == 60);
    REQUIRE(changed.notes[1].pitch == 64);

    phrase.notes[0].pitch = 0;
    phrase.notes[1].pitch = 127;
    PresetMidiEditor bounded(phrase, [&](PresetMidiSequence) { ++publications; });
    bounded.keyPressed(KeyPress(KeyPress::upKey, ModifierKeys::shiftModifier, 0));
    bounded.keyPressed(KeyPress(KeyPress::downKey, ModifierKeys::shiftModifier, 0));
    REQUIRE(publications == 2);
}

TEST_CASE("Creating and dragging moves a fixed length note and hands off audition",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    PresetMidiSequence changed;
    int publications = 0;
    std::vector<std::pair<int, bool>> audition;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
        changed = std::move(next);
        ++publications;
    }, {}, [&](int pitch, int, bool on) { audition.emplace_back(pitch, on); });
    const float y = pitch60Y(editor);
    const float nextY = editor.pitchRow(61).getCentreY();
    editor.mouseDown(pointer(editor, {xAt(editor, 0.5, 4.0), y}));
    editor.mouseDrag(pointer(editor, {xAt(editor, 0.75, 4.0), nextY}));
    REQUIRE(publications == 0);
    editor.mouseUp(pointer(editor, {xAt(editor, 0.75, 4.0), nextY}));
    REQUIRE(publications == 1);
    REQUIRE(changed.notes.size() == 1);
    REQUIRE(changed.notes[0].pitch == 61);
    REQUIRE(changed.notes[0].startSeconds == 0.75);
    REQUIRE(changed.notes[0].durationSeconds == 0.125);
    const std::vector<std::pair<int, bool>> expected {{60, true}, {60, false}, {61, true}, {61, false}};
    REQUIRE(audition == expected);
}

TEST_CASE("Notes extend the phrase while the drag projection remains fixed",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes = {{60, 90, 3.5, 0.5}};
    PresetMidiSequence changed;
    int publications = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
        changed = std::move(next);
        ++publications;
    });
    const auto map = editor.miniMap();
    editor.mouseDown(pointer(editor, {map.getRight() - 1.f, map.getCentreY()}));
    editor.mouseUp(pointer(editor, {map.getRight() - 1.f, map.getCentreY()}));
    const auto grid = editor.noteGrid();
    const float y = pitch60Y(editor);
    const float x = grid.getX() + grid.getWidth() * 0.8125f;
    editor.mouseDown(pointer(editor, {x, y}));
    editor.mouseDrag(pointer(editor, {x + grid.getWidth() * 0.25f, y}));
    editor.mouseDrag(pointer(editor, {x + grid.getWidth() * 0.5f, y}));
    REQUIRE(publications == 0);
    editor.mouseUp(pointer(editor, {x + grid.getWidth() * 0.5f, y}));
    REQUIRE(publications == 1);
    REQUIRE(changed.notes[0].startSeconds == 4.5);
    REQUIRE(changed.notes[0].durationSeconds == 0.5);
    REQUIRE(changed.durationSeconds == 5.0);
}

TEST_CASE("Snap pill permits continuous note movement and short resizing",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes = {{60, 90, 0.5, 0.5}};
    PresetMidiSequence changed;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) { changed = std::move(next); });
    ToggleButton* snap = nullptr;
    for (auto* child : editor.getChildren()) {
        if (child->getName() == "PresetMidiEditor.Snap") {
            snap = dynamic_cast<ToggleButton*>(child);
        }
    }
    REQUIRE(snap != nullptr);
    REQUIRE(snap->getToggleState());
    snap->setToggleState(false, dontSendNotification);
    const float y = pitch60Y(editor);
    const float x = xAt(editor, 0.625, 4.0);
    const float delta = editor.noteGrid().getWidth() * 0.037f / 2.f;
    editor.mouseDown(pointer(editor, {x, y}));
    editor.mouseDrag(pointer(editor, {x + delta, y}));
    editor.mouseUp(pointer(editor, {x + delta, y}));
    REQUIRE(std::abs(changed.notes[0].startSeconds - 0.537) < 0.00001);
    const float grip = xAt(editor, 1.037, 4.0) - 3.f;
    editor.mouseDown(pointer(editor, {grip, y}));
    editor.mouseDrag(pointer(editor, {grip - editor.noteGrid().getWidth(), y}));
    editor.mouseUp(pointer(editor, {grip - editor.noteGrid().getWidth(), y}));
    REQUIRE(changed.notes[0].durationSeconds == 0.01);
    const auto lane = editor.controlGrid();
    editor.mouseDown(pointer(editor, {xAt(editor, 0.513, 4.0), lane.getCentreY()}));
    editor.mouseUp(pointer(editor, {xAt(editor, 0.513, 4.0), lane.getCentreY()}));
    REQUIRE(std::abs(changed.controls[0].timeSeconds - 0.513) < 0.00001);
}

TEST_CASE("Created and resized notes grow the pattern beyond its previous endpoint",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    PresetMidiSequence changed;
    int publications = 0;
    SECTION("Create a note at the old endpoint") {
        PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
            changed = std::move(next);
            ++publications;
        });
        const auto map = editor.miniMap();
        editor.mouseDown(pointer(editor, {map.getRight() - 1.f, map.getCentreY()}));
        editor.mouseUp(pointer(editor, {map.getRight() - 1.f, map.getCentreY()}));
        const auto grid = editor.noteGrid();
        editor.mouseDown(pointer(editor, {grid.getRight() - 1.f, grid.getCentreY()}));
        editor.mouseUp(pointer(editor, {grid.getRight() - 1.f, grid.getCentreY()}));
        REQUIRE(changed.notes[0].startSeconds == 4.0);
        REQUIRE(changed.durationSeconds == 4.125);
    }
    SECTION("Resize a note past the old endpoint") {
        phrase.notes = {{60, 90, 1.0, 0.5}};
        PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
            changed = std::move(next);
            ++publications;
        });
        const float x = xAt(editor, 1.5, 4.0) - 3.f;
        const float y = pitch60Y(editor);
        const float delta = editor.noteGrid().getWidth() * 1.5f;
        editor.mouseDown(pointer(editor, {x, y}));
        editor.mouseDrag(pointer(editor, {x + delta, y}));
        editor.mouseUp(pointer(editor, {x + delta, y}));
        REQUIRE(changed.notes[0].startSeconds == 1.0);
        REQUIRE(changed.notes[0].durationSeconds == 3.5);
        REQUIRE(changed.durationSeconds == 4.5);
    }
    REQUIRE(publications == 1);
}

TEST_CASE("Ruler and overview scrub a visible stopped playhead without authoring events",
        "[cycle-v2][preset][sequence][editor][seek]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    int publications = 0;
    int auditions = 0;
    std::vector<double> positions;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence) { ++publications; }, {},
            [&](int, int, bool) { ++auditions; }, {}, {},
            [&](double time) { positions.push_back(time); });
    const auto ruler = editor.playbackRuler();
    const float x = xAt(editor, 0.513, 4.0);
    editor.mouseDown(pointer(editor, {x, ruler.getCentreY()}));
    editor.mouseDrag(pointer(editor, {x + editor.noteGrid().getWidth() * 0.25f, ruler.getCentreY()}));
    editor.mouseUp(pointer(editor, {x, ruler.getCentreY()}));
    REQUIRE(positions.size() == 2);
    REQUIRE(std::abs(positions[0] - 0.513) < 0.00001);
    REQUIRE(std::abs(positions[1] - 1.013) < 0.00001);
    const auto map = editor.miniMap();
    editor.mouseDown(pointer(editor, {map.getCentreX(), map.getY() + 1.f}));
    editor.mouseDrag(pointer(editor, {map.getX() + map.getWidth() * 0.75f, map.getBottom() - 1.f}));
    editor.mouseUp(pointer(editor, {map.getCentreX(), map.getCentreY()}));
    REQUIRE(positions.back() == 3.0);
    REQUIRE(publications == 0);
    REQUIRE(auditions == 0);
    const Image image = editor.createComponentSnapshot(editor.getLocalBounds());
    const int cursorX = roundToInt(map.getX() + map.getWidth() * 0.75f);
    REQUIRE(image.getPixelAt(cursorX, roundToInt(map.getCentreY())).getBrightness() > 0.5f);
    const auto grid = editor.noteGrid();
    editor.mouseDown(pointer(editor, {grid.getX() + 2.f, pitch60Y(editor)}));
    editor.mouseUp(pointer(editor, {grid.getX() + 2.f, pitch60Y(editor)}));
    REQUIRE(publications == 1);
}

TEST_CASE("Grid and snapping follow saved tempo independently of saved duration",
        "[cycle-v2][preset][sequence][editor][tempo]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 7.0;
    phrase.tempoBpm = 100.0;
    phrase.notes = {{60, 90, 0.6, 0.6}};
    PresetMidiSequence changed;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) { changed = std::move(next); });
    Label* tempo = nullptr;
    for (auto* child : editor.getChildren()) {
        if (child->getName() == "PresetMidiEditor.Tempo") {
            tempo = dynamic_cast<Label*>(child);
        }
    }
    REQUIRE(tempo != nullptr);
    const auto grid = editor.noteGrid();
    const int beatX = roundToInt(grid.getX() + grid.getWidth() * 0.25f);
    const int gridY = roundToInt(grid.getY() + editor.pitchRow(60).getHeight() * 0.5f);
    const Image before = editor.createComponentSnapshot(editor.getLocalBounds());
    const auto place = [&](PresetMidiEditor& roll) {
        const auto area = roll.noteGrid();
        roll.mouseDown(pointer(roll, {area.getX() + area.getWidth() * 0.75f, pitch60Y(roll)}));
        roll.mouseUp(pointer(roll, {area.getX() + area.getWidth() * 0.75f, pitch60Y(roll)}));
    };
    place(editor);
    REQUIRE(std::abs(changed.notes.back().startSeconds - 1.8) < 0.00001);
    REQUIRE(std::abs(changed.notes.back().durationSeconds - 0.15) < 0.00001);
    REQUIRE(changed.notes[0].startSeconds == 0.6);
    REQUIRE(changed.durationSeconds == 7.0);
    PresetMidiEditor reopened(changed, [&](PresetMidiSequence next) { changed = std::move(next); });
    const Image reopenedImage = reopened.createComponentSnapshot(reopened.getLocalBounds());
    REQUIRE(reopenedImage.getPixelAt(beatX, gridY) == before.getPixelAt(beatX, gridY));
    reopened.mouseDown(pointer(reopened, {grid.getX() + grid.getWidth() * 0.875f, pitch60Y(reopened)}));
    reopened.mouseUp(pointer(reopened, {grid.getX() + grid.getWidth() * 0.875f, pitch60Y(reopened)}));
    REQUIRE(std::abs(changed.notes.back().startSeconds - 2.1) < 0.00001);
    tempo->setText("120.0", sendNotificationSync);
    REQUIRE(changed.tempoBpm == 120.0);
    REQUIRE(changed.notes[0].startSeconds == 0.6);
}

TEST_CASE("New notes inherit the last touched note length and edited velocity",
        "[cycle-v2][preset][sequence][editor][note-template]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes = {{60, 82, 0.25, 0.5}};
    PresetMidiSequence changed;
    int publications = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
        changed = std::move(next);
        ++publications;
    });
    const float y = pitch60Y(editor);
    double expectedDuration = 0.5;
    int expectedVelocity = 82;
    SECTION("Touching an existing note chooses its shape") {
        const float x = xAt(editor, 0.375, 4.0);
        editor.mouseDown(pointer(editor, {x, y}));
        editor.mouseUp(pointer(editor, {x, y}));
    }
    SECTION("Resizing and a velocity lane edit update the shape") {
        const float grip = xAt(editor, 0.75, 4.0) - 3.f;
        const float delta = editor.noteGrid().getWidth() / 8.f;
        editor.mouseDown(pointer(editor, {grip, y}));
        editor.mouseDrag(pointer(editor, {grip + delta, y}));
        editor.mouseUp(pointer(editor, {grip + delta, y}));
        expectedDuration = 0.75;
        const auto lane = editor.velocityGrid();
        const float x = xAt(editor, 0.25, 4.0);
        const float velocityY = lane.getBottom() - lane.getHeight() * 105.f / 127.f;
        editor.mouseDown(pointer(editor, {x, velocityY}));
        editor.mouseUp(pointer(editor, {x, velocityY}));
        expectedVelocity = 105;
    }
    SECTION("Keyboard velocity edits update the shape") {
        const auto lane = editor.velocityGrid();
        const float x = xAt(editor, 0.25, 4.0);
        const float velocityY = lane.getBottom() - lane.getHeight() * 82.f / 127.f;
        editor.mouseDown(pointer(editor, {x, velocityY}));
        editor.mouseUp(pointer(editor, {x, velocityY}));
        REQUIRE(editor.keyPressed(KeyPress(KeyPress::upKey)));
        expectedVelocity = 83;
    }
    const int committed = publications;
    const float nextY = editor.pitchRow(62).getCentreY();
    const float x = xAt(editor, 1.25, 4.0);
    editor.mouseDown(pointer(editor, {x, nextY}));
    editor.mouseDrag(pointer(editor, {xAt(editor, 1.375, 4.0), nextY}));
    REQUIRE(publications == committed);
    editor.mouseUp(pointer(editor, {xAt(editor, 1.375, 4.0), nextY}));
    REQUIRE(publications == committed + 1);
    REQUIRE(changed.notes.size() == 2);
    REQUIRE(changed.notes.back().durationSeconds == expectedDuration);
    REQUIRE(changed.notes.back().velocity == expectedVelocity);
    REQUIRE(changed.notes.back().pitch == 62);
    REQUIRE(changed.notes.back().startSeconds == 1.375);
}

TEST_CASE("Piano roll keeps two tempo-aware measures available beyond authored content",
        "[cycle-v2][preset][sequence][editor][scroll-tail]") {
    ScopedJuceInitialiser_GUI gui;
    for (const double bpm : { 60.0, 120.0, 240.0 }) {
        PresetMidiSequence phrase;
        phrase.tempoBpm = bpm;
        phrase.durationSeconds = 4.0;
        phrase.notes.push_back({ 60, 100, 2.5, 0.5 });
        PresetMidiSequence changed;
        int publications = 0;
        PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
            changed = std::move(next);
            ++publications;
        });
        const auto grid = editor.noteGrid();
        const Point<float> emptyRow(grid.getX() + 0.01f, grid.getY() + grid.getHeight() / 48.f);
        MouseWheelDetails wheel {};
        wheel.deltaX = -1000.f;
        editor.mouseWheelMove(pointer(editor, emptyRow), wheel);
        REQUIRE(publications == 0);
        editor.mouseDown(pointer(editor, emptyRow));
        REQUIRE(publications == 0);
        editor.mouseUp(pointer(editor, emptyRow));
        REQUIRE(publications == 1);
        REQUIRE(changed.notes.size() == 2);
        const double measure = 240.0 / bpm;
        REQUIRE(std::abs(changed.notes.back().startSeconds - (3.0 + measure)) < 0.00001);
        REQUIRE(std::abs(changed.durationSeconds - (3.0 + measure + 15.0 / bpm)) < 0.00001);

        editor.mouseWheelMove(pointer(editor, emptyRow), wheel);
        REQUIRE(publications == 1);
        editor.mouseDown(pointer(editor, emptyRow));
        editor.mouseUp(pointer(editor, emptyRow));
        REQUIRE(publications == 2);
        REQUIRE(std::abs(changed.notes.back().startSeconds - (3.0 + 2.0 * measure + 15.0 / bpm)) < 0.00001);
    }
}

TEST_CASE("Piano-roll scroll space respects automation tails empty patterns and duration cap",
        "[cycle-v2][preset][sequence][editor][scroll-tail]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes.push_back({ 60, 100, 2.5, 0.5 });
    double expectedStart = 12.0;
    SECTION("Automation after the final note remains reachable with two measures beyond it") {
        phrase.controls.push_back({ 1, 80, 10.0 });
    }
    SECTION("Empty patterns have two blank measures without a floating-point extra grid step") {
        phrase.notes.clear();
        phrase.durationSeconds = 0.01;
        expectedStart = 2.0;
    }
    SECTION("Scrolling cannot author beyond the existing duration cap") {
        phrase.durationSeconds = PresetMidiSequence::maximumDurationSeconds;
        phrase.notes[0].startSeconds = phrase.durationSeconds - 0.5;
        expectedStart = phrase.durationSeconds - 0.125;
    }
    PresetMidiSequence changed;
    int publications = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
        changed = std::move(next);
        ++publications;
    });
    const auto grid = editor.noteGrid();
    Point<float> point(grid.getX() + 0.01f, grid.getY() + grid.getHeight() / 48.f);
    if (phrase.durationSeconds == PresetMidiSequence::maximumDurationSeconds) {
        point.x = grid.getRight() - 0.01f;
    }
    MouseWheelDetails wheel {};
    wheel.deltaX = -1000.f;
    editor.mouseWheelMove(pointer(editor, point), wheel);
    REQUIRE(publications == 0);
    editor.mouseDown(pointer(editor, point));
    editor.mouseUp(pointer(editor, point));
    REQUIRE(publications == 1);
    REQUIRE(std::abs(changed.notes.back().startSeconds - expectedStart) < 0.00001);
    REQUIRE(changed.durationSeconds <= PresetMidiSequence::maximumDurationSeconds);
}

TEST_CASE("Backspace returns to the phrase start without deleting the selection",
        "[cycle-v2][preset][sequence][editor][seek]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes.push_back({ 60, 100, 1.0, 0.5 });
    phrase.controls.push_back({ 1, 40, 1.0 });
    PresetMidiSequence changed;
    int publications = 0;
    std::vector<double> positions;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
        changed = std::move(next);
        ++publications;
    }, {}, {}, {}, {}, [&](double position) { positions.push_back(position); });
    bool controlSelected = false;
    SECTION("Selected note remains available to Delete") {
        const Point<float> note(xAt(editor, 1.2, 4.0), pitch60Y(editor));
        editor.mouseDown(pointer(editor, note));
        editor.mouseUp(pointer(editor, note));
    }
    SECTION("Selected automation point remains available to Delete") {
        controlSelected = true;
        const auto lane = editor.controlGrid();
        const Point<float> point(xAt(editor, 1.0, 4.0), lane.getBottom() - lane.getHeight() * 40.f / 127.f);
        editor.mouseDown(pointer(editor, point));
        editor.mouseUp(pointer(editor, point));
    }
    const auto map = editor.miniMap();
    const Point<float> seek(map.getX() + map.getWidth() * 0.75f, map.getCentreY());
    editor.mouseDown(pointer(editor, seek));
    editor.mouseUp(pointer(editor, seek));
    REQUIRE(positions.back() == 3.0);
    MouseWheelDetails wheel {};
    wheel.deltaX = -1000.f;
    editor.mouseWheelMove(pointer(editor, editor.noteGrid().getCentre()), wheel);
    const int before = publications;
    REQUIRE(editor.keyPressed(KeyPress(KeyPress::backspaceKey)));
    REQUIRE(positions.back() == 0.0);
    REQUIRE(publications == before);
    REQUIRE(editor.keyPressed(KeyPress(KeyPress::deleteKey)));
    REQUIRE(publications == before + 1);
    REQUIRE((controlSelected ? changed.controls.empty() : changed.notes.empty()));
    const auto grid = editor.noteGrid();
    const Point<float> start(grid.getX() + 0.01f, grid.getY() + grid.getHeight() / 48.f);
    editor.mouseDown(pointer(editor, start));
    editor.mouseUp(pointer(editor, start));
    REQUIRE(changed.notes.back().startSeconds == 0.0);
}

TEST_CASE("Left and Right seek one measure relative to the live phrase cursor",
        "[cycle-v2][preset][sequence][editor][seek]") {
    ScopedJuceInitialiser_GUI gui;
    for (const double bpm : { 60.0, 100.0, 240.0 }) {
        for (const bool playing : { false, true }) {
            PresetMidiSequence phrase;
            phrase.durationSeconds = 10.0;
            phrase.tempoBpm = bpm;
            phrase.notes.push_back({ 60, 100, 1.0, 0.5 });
            PresetMidiSequence changed;
            int publications = 0;
            int toggles = 0;
            double position = 0.75;
            PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
                changed = std::move(next);
                ++publications;
            }, [&] { ++toggles; }, {}, [&]() -> std::optional<double> {
                return playing ? std::optional<double>(position) : std::nullopt;
            }, {}, [&](double target) { position = target; }, position);
            const double measure = 240.0 / bpm;
            REQUIRE(editor.keyPressed(KeyPress(KeyPress::rightKey)));
            REQUIRE(std::abs(position - (0.75 + measure)) < 0.00001);
            REQUIRE(editor.keyPressed(KeyPress(KeyPress::leftKey)));
            REQUIRE(std::abs(position - 0.75) < 0.00001);
            for (int step = 0; step < 12; ++step) {
                REQUIRE(editor.keyPressed(KeyPress(KeyPress::rightKey)));
            }
            REQUIRE(position == 10.0);
            REQUIRE(editor.keyPressed(KeyPress(KeyPress::leftKey)));
            REQUIRE(std::abs(position - (10.0 - measure)) < 0.00001);
            for (int step = 0; step < 12; ++step) {
                REQUIRE(editor.keyPressed(KeyPress(KeyPress::leftKey)));
            }
            REQUIRE(position == 0.0);
            REQUIRE(publications == 0);
            REQUIRE(toggles == 0);
            const auto grid = editor.noteGrid();
            const Point<float> start(grid.getX() + 0.01f, grid.getY() + grid.getHeight() / 48.f);
            editor.mouseDown(pointer(editor, start));
            editor.mouseUp(pointer(editor, start));
            REQUIRE(publications == 1);
            REQUIRE(changed.notes.back().startSeconds == 0.0);
        }
    }
}

TEST_CASE("Compact editor aligns plots and exposes 36 editable semitone rows",
        "[cycle-v2][preset][sequence][editor][ui]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes = {{60, 100, 0.0, 0.125}};
    PresetMidiSequence changed;
    int publications = 0;
    for (const Point<int> size : {Point<int>(1120, 820), Point<int>(900, 640)}) {
        PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
            changed = std::move(next);
            ++publications;
        });
        editor.setSize(size.x, size.y);
        for (auto* child : editor.getChildren()) {
            if (child->getName() == "PresetMidiEditor.Tempo"
                    || child->getName() == "PresetMidiEditor.Snap"
                    || dynamic_cast<TextButton*>(child) != nullptr) {
                REQUIRE(child->getY() == 8);
                REQUIRE(child->getHeight() == 28);
                REQUIRE(child->getBounds().getCentreY() == 22);
            }
            if (child->getName() == "PresetMidiEditor.Tempo") {
                REQUIRE(child->getX() == 16);
            }
            if (dynamic_cast<TextButton*>(child) != nullptr) {
                REQUIRE(editor.getWidth() - child->getRight() == 16);
            }
        }
        const auto roll = editor.noteGrid();
        const auto velocity = editor.velocityGrid();
        const auto cc = editor.controlGrid();
        REQUIRE(roll.getX() == 80.f);
        REQUIRE((float) editor.getWidth() - roll.getRight() == 16.f);
        for (const auto plot : {editor.miniMap(), editor.playbackRuler(), velocity, cc}) {
            REQUIRE(plot.getX() == roll.getX());
            REQUIRE(plot.getRight() == roll.getRight());
        }
        REQUIRE(velocity.getY() - roll.getBottom() == 4.f);
        REQUIRE(cc.getY() - velocity.getBottom() == 4.f);
        REQUIRE((float) editor.getHeight() - cc.getBottom() == 4.f);
        REQUIRE(std::abs(roll.getHeight() / editor.pitchRow(60).getHeight() - 36.f) < 0.001f);
        for (int pitch = 42; pitch <= 77; ++pitch) {
            const auto row = editor.pitchRow(pitch);
            REQUIRE(roll.contains(row));
            const Point<float> at(roll.getX() + roll.getWidth() * 0.75f, row.getCentreY());
            editor.mouseDown(pointer(editor, at));
            editor.mouseUp(pointer(editor, at));
            REQUIRE(changed.notes.back().pitch == pitch);
            REQUIRE(changed.notes.back().startSeconds == 1.5);
        }
    }
    REQUIRE(publications == 72);
}

TEST_CASE("Overview excludes automation while controller lanes retain it",
        "[cycle-v2][preset][sequence][editor][ui]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence notes;
    notes.notes = {{60, 100, 0.25, 0.5}};
    auto automated = notes;
    automated.controls = {{1, 8, 0.0}, {1, 120, 1.0}, {1, 40, 2.0}};
    PresetMidiEditor plain(notes, {});
    PresetMidiEditor withControls(automated, {});
    const auto a = plain.createComponentSnapshot(plain.getLocalBounds());
    const auto b = withControls.createComponentSnapshot(withControls.getLocalBounds());
    const auto overview = plain.miniMap().toNearestInt();
    for (int y = overview.getY(); y < overview.getBottom(); ++y) {
        for (int x = overview.getX(); x < overview.getRight(); ++x) {
            if (a.getPixelAt(x, y) != b.getPixelAt(x, y)) {
                FAIL("Automation changed the notes-only overview");
            }
        }
    }
    const auto cc = plain.controlGrid().toNearestInt();
    bool differs = false;
    for (int y = cc.getY(); y < cc.getBottom() && !differs; ++y) {
        for (int x = cc.getX(); x < cc.getRight(); ++x) {
            differs |= a.getPixelAt(x, y) != b.getPixelAt(x, y);
        }
    }
    REQUIRE(differs);
}

TEST_CASE("Overflowing controller lanes scroll and remain editable in compact windows",
        "[cycle-v2][preset][sequence][editor][ui]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.notes = {{60, 100, 0.0, 0.5}};
    PresetMidiSequence changed;
    int publications = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence next) {
        changed = std::move(next);
        ++publications;
    }, {}, {}, {}, {1, 2, 7, 11, 64, 71, 73, 74});
    editor.setSize(900, 640);
    ScrollBar* scroll = nullptr;
    for (auto* child : editor.getChildren()) {
        if (auto* bar = dynamic_cast<ScrollBar*>(child)) {
            scroll = bar;
        }
    }
    REQUIRE(scroll != nullptr);
    REQUIRE(scroll->isVisible());
    const auto roll = editor.noteGrid();
    const auto velocity = editor.velocityGrid();
    const auto first = editor.controlGrid(1);
    REQUIRE(editor.controlGrid(74).getY() > (float) editor.getHeight());
    MouseWheelDetails wheel {};
    wheel.deltaY = -100.f;
    for (int step = 0; step < 10; ++step) {
        editor.mouseWheelMove(pointer(editor, first.getCentre()), wheel);
    }
    REQUIRE(scroll->getCurrentRangeStart() > 0.0);
    REQUIRE(editor.noteGrid() == roll);
    REQUIRE(editor.velocityGrid() == velocity);
    const auto last = editor.controlGrid(74);
    REQUIRE(last.getBottom() == 636.f);
    const auto point = last.getRelativePoint(0.5f, 0.5f);
    editor.mouseDown(pointer(editor, point));
    editor.mouseDrag(pointer(editor, point.translated(20.f, -10.f)));
    REQUIRE(publications == 0);
    editor.mouseUp(pointer(editor, point));
    REQUIRE(publications == 1);
    REQUIRE(changed.controls.back().controller == 74);
    REQUIRE(changed.controls.back().value > 64);
    editor.setSize(1120, 1200);
    REQUIRE(editor.controlGrid(74).getBottom() <= 1196.f);
}
