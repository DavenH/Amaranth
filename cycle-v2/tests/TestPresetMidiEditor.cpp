#include <catch2/catch_test_macros.hpp>

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

float xAt(const PresetMidiEditor& editor, double time, double duration) {
    const auto grid = editor.noteGrid();
    return grid.getX() + grid.getWidth()
            * (float) (time / (duration * 0.5));
}

float pitch60Y(const PresetMidiEditor& editor) {
    const auto grid = editor.noteGrid();
    return grid.getY() + grid.getHeight() * 11.5f / 24.f;
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

TEST_CASE("Trackpad deltas accumulate and the minimap navigates the snapped viewport",
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
    REQUIRE(changed.notes[0].pitch == 61);
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
    REQUIRE(changed.notes.back().pitch > changed.notes.front().pitch);
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
    REQUIRE(editor.controlGrid().getHeight() >= 90.f);
    REQUIRE(editor.velocityGrid().getHeight() >= 65.f);
    REQUIRE(editor.miniMap().getWidth() == editor.noteGrid().getWidth());
    const Image image = editor.createComponentSnapshot(editor.getLocalBounds());
    const int cursorX = roundToInt(xAt(editor, 1.0, 4.0));
    const Colour cursor = image.getPixelAt(cursorX,
            roundToInt(editor.noteGrid().getY() + 20.f));
    REQUIRE(cursor.getGreen() > 150);
    REQUIRE(cursor.getRed() < 100);
    const auto grid = editor.noteGrid();
    const int stripeX = roundToInt(grid.getX() + 24.f);
    const float row = grid.getHeight() / 24.f;
    const Colour whiteRow = image.getPixelAt(stripeX,
            roundToInt(grid.getY() + row * 0.5f));
    const Colour blackRow = image.getPixelAt(stripeX,
            roundToInt(grid.getY() + row * 1.5f));
    REQUIRE(whiteRow.getBrightness() > blackRow.getBrightness());
}

TEST_CASE("Roll length is editable independently of the final note",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes.push_back({ 60, 100, 0.0, 0.5 });
    PresetMidiSequence changed;
    int publications = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence sequence) {
        changed = std::move(sequence);
        ++publications;
    });
    Slider* length = nullptr;
    for (int index = 0; index < editor.getNumChildComponents(); ++index) {
        if (auto* slider = dynamic_cast<Slider*>(editor.getChildComponent(index))) {
            if (slider->getName() == "PresetMidiEditor.Length") {
                length = slider;
            }
        }
    }
    REQUIRE(length != nullptr);
    length->setValue(6.0, sendNotificationSync);
    REQUIRE(publications == 1);
    REQUIRE(changed.durationSeconds == 6.0);
    REQUIRE(changed.notes[0].durationSeconds == 0.5);
}
