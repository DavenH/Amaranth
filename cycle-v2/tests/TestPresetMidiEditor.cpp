#include <catch2/catch_test_macros.hpp>

#include "UI/PresetMidiEditor.h"

using namespace CycleV2;
using namespace juce;

namespace {

MouseEvent pointer(Component& component, Point<float> position,
        ModifierKeys modifiers = ModifierKeys::leftButtonModifier) {
    const Time now = Time::getCurrentTime();
    return {
            Desktop::getInstance().getMainMouseSource(),
            position,
            modifiers,
            1.f,
            0.f,
            0.f,
            0.f,
            0.f,
            &component,
            &component,
            now,
            position,
            now,
            1,
            false
    };
}

}

TEST_CASE("Piano roll edits a note gesture and CC envelope through real pointer events",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes.push_back({ 60, 100, 1.0, 1.0 });
    PresetMidiSequence changed;
    int publications = 0;
    int transportToggles = 0;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence sequence) {
        changed = std::move(sequence);
        ++publications;
    }, [&] { ++transportToggles; });

    editor.mouseDown(pointer(editor, { 290.f, 134.f }));
    editor.mouseDrag(pointer(editor, { 344.f, 134.f }));
    REQUIRE(publications == 0);
    editor.mouseUp(pointer(editor, { 344.f, 134.f }));
    REQUIRE(publications == 1);
    REQUIRE(changed.notes.size() == 1);
    REQUIRE(changed.notes[0].pitch == 60);
    REQUIRE(changed.notes[0].startSeconds == 1.25);

    editor.mouseDown(pointer(editor, { 160.f, 277.f }));
    editor.mouseUp(pointer(editor, { 160.f, 277.f }));
    REQUIRE(publications == 2);
    REQUIRE(changed.controls.size() == 1);
    editor.mouseDown(pointer(editor, { 390.f, 277.f }));
    editor.mouseUp(pointer(editor, { 390.f, 277.f }));
    REQUIRE(publications == 3);
    REQUIRE(changed.controls.size() == 2);
    editor.mouseDown(pointer(editor, { 390.f, 277.f }));
    editor.mouseDrag(pointer(editor, { 420.f, 265.f }));
    editor.mouseUp(pointer(editor, { 420.f, 265.f }));
    REQUIRE(publications == 4);
    REQUIRE(changed.controls[1].value > changed.controls[0].value);

    REQUIRE(editor.keyPressed(KeyPress(KeyPress::deleteKey)));
    REQUIRE(changed.controls.size() == 1);
    editor.mouseDown(pointer(editor, { 160.f, 277.f },
            ModifierKeys::rightButtonModifier));
    REQUIRE(changed.controls.empty());
    REQUIRE(editor.keyPressed(KeyPress(KeyPress::spaceKey)));
    REQUIRE(transportToggles == 1);
    editor.mouseDown(pointer(editor, { 350.f, 134.f }));
    REQUIRE(editor.keyPressed(KeyPress(KeyPress::deleteKey)));
    REQUIRE(changed.notes.empty());
}

TEST_CASE("Two-finger scrolling pans the note and CC viewport together",
        "[cycle-v2][preset][sequence][editor]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    PresetMidiSequence changed;
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence sequence) {
        changed = std::move(sequence);
    });
    MouseWheelDetails wheel {};
    wheel.deltaX = -0.5f;
    editor.mouseWheelMove(pointer(editor, { 200.f, 150.f }), wheel);
    editor.mouseDown(pointer(editor, { 100.f, 140.f }));
    editor.mouseUp(pointer(editor, { 100.f, 140.f }));
    REQUIRE(changed.notes.size() == 1);
    REQUIRE(changed.notes[0].startSeconds >= 1.0);
    editor.mouseDown(pointer(editor, { 100.f, 280.f }));
    editor.mouseUp(pointer(editor, { 100.f, 280.f }));
    REQUIRE(changed.controls.size() == 1);
    REQUIRE(changed.controls[0].timeSeconds >= 1.0);
}
