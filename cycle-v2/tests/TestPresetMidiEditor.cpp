#include <catch2/catch_test_macros.hpp>

#include "UI/PresetMidiEditor.h"

using namespace CycleV2;
using namespace juce;

namespace {

MouseEvent pointer(Component& component, Point<float> position) {
    const Time now = Time::getCurrentTime();
    return {
            Desktop::getInstance().getMainMouseSource(),
            position,
            ModifierKeys::leftButtonModifier,
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
    PresetMidiEditor editor(phrase, [&](PresetMidiSequence sequence) {
        changed = std::move(sequence);
        ++publications;
    });

    editor.mouseDown(pointer(editor, { 175.f, 134.f }));
    editor.mouseDrag(pointer(editor, { 233.f, 134.f }));
    REQUIRE(publications == 0);
    editor.mouseUp(pointer(editor, { 233.f, 134.f }));
    REQUIRE(publications == 1);
    REQUIRE(changed.notes.size() == 1);
    REQUIRE(changed.notes[0].pitch == 60);
    REQUIRE(changed.notes[0].startSeconds == 1.5);

    editor.mouseDown(pointer(editor, { 330.f, 134.f }));
    editor.mouseDrag(pointer(editor, { 389.f, 134.f }));
    editor.mouseUp(pointer(editor, { 389.f, 134.f }));
    REQUIRE(publications == 2);
    REQUIRE(changed.notes[0].durationSeconds == 1.5);

    editor.mouseDown(pointer(editor, { 160.f, 277.f }));
    editor.mouseDrag(pointer(editor, { 218.f, 266.f }));
    editor.mouseUp(pointer(editor, { 218.f, 266.f }));
    REQUIRE(publications == 3);
    REQUIRE(changed.controls.size() == 2);
    REQUIRE(changed.controls[0].controller == 1);
    REQUIRE(changed.controls[1].value > changed.controls[0].value);

    REQUIRE(editor.keyPressed(KeyPress(KeyPress::deleteKey)));
    REQUIRE(changed.notes.empty());
}
