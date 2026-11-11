#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "UI/PresetMidiEditor.h"
#include "UI/MidiPatternEditorWindow.h"

using namespace CycleV2;
using namespace juce;

namespace {

MouseEvent pointer(Component& component, Point<float> point, int flags) {
    const auto now = Time::getCurrentTime();
    return { Desktop::getInstance().getMainMouseSource(), point, ModifierKeys(flags),
            1.f, 0.f, 0.f, 0.f, 0.f, &component, &component, now, point, now, 1, false };
}

}

TEST_CASE("MIDI box selection moves only selected notes and publishes on release",
        "[cycle-v2][midi-editing]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.notes = {{60, 100, 0.25, 0.125}, {64, 80, 0.5, 0.125}, {72, 90, 1.5, 0.25}};
    PresetMidiSequence changed;
    int publications = 0;
    PresetMidiEditor editor(phrase, [&](auto next) {
        changed = next;
        ++publications;
    });
    const auto grid = editor.noteGrid();
    const int shiftDrag = ModifierKeys::leftButtonModifier | ModifierKeys::shiftModifier;
    const Point<float> start { grid.getX() + 1.f, editor.pitchRow(64).getY() - 1.f };
    const Point<float> end { grid.getX() + grid.getWidth() * 0.4f, editor.pitchRow(60).getBottom() + 1.f };
    editor.mouseDown(pointer(editor, start, shiftDrag));
    editor.mouseDrag(pointer(editor, end, shiftDrag));
    editor.mouseUp(pointer(editor, end, shiftDrag));
    REQUIRE(publications == 0);
    editor.keyPressed(KeyPress(KeyPress::leftKey, ModifierKeys::shiftModifier, 0));
    REQUIRE(publications == 0);
    REQUIRE(editor.keyPressed(KeyPress(KeyPress::rightKey, ModifierKeys::shiftModifier, 0)));
    REQUIRE(publications == 1);
    REQUIRE(changed.notes[0].startSeconds == Catch::Approx(0.75));
    REQUIRE(changed.notes[1].startSeconds == Catch::Approx(1.0));
    REQUIRE(changed.notes[2].startSeconds == Catch::Approx(1.5));
    REQUIRE(editor.keyPressed(KeyPress(KeyPress::upKey, ModifierKeys::shiftModifier, 0)));
    REQUIRE(changed.notes[0].pitch == 61);
    REQUIRE(changed.notes[1].pitch == 65);
    REQUIRE(changed.notes[2].pitch == 72);
    const Point<float> down { grid.getX() + grid.getWidth() * 0.4f,
            editor.pitchRow(61).getCentreY() };
    const auto middle = down.translated(grid.getWidth() * 0.0625f, 0.f);
    const auto finish = down.translated(grid.getWidth() * 0.125f, 0.f);
    const int beforeDrag = publications;
    editor.mouseDown(pointer(editor, down, ModifierKeys::leftButtonModifier));
    editor.mouseDrag(pointer(editor, middle, ModifierKeys::leftButtonModifier));
    editor.mouseDrag(pointer(editor, finish, ModifierKeys::leftButtonModifier));
    REQUIRE(publications == beforeDrag);
    editor.mouseUp(pointer(editor, finish, ModifierKeys::leftButtonModifier));
    REQUIRE(publications == beforeDrag + 1);
    REQUIRE(changed.notes[0].startSeconds == Catch::Approx(1.0));
    REQUIRE(changed.notes[1].startSeconds == Catch::Approx(1.25));
    REQUIRE(changed.notes[2].startSeconds == Catch::Approx(1.5));
    REQUIRE(editor.keyPressed(KeyPress(KeyPress::deleteKey)));
    REQUIRE(changed.notes.size() == 1);
    REQUIRE(changed.notes[0].pitch == 72);
    const int before = publications;
    editor.keyPressed(KeyPress(KeyPress::upKey, ModifierKeys::shiftModifier, 0));
    REQUIRE(publications == before);
}

TEST_CASE("MIDI pattern draft saves only on request and cancellation retains the draft",
        "[cycle-v2][midi-editing][pattern]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.notes = {{60, 100, 0.0, 0.25}};
    int saves = 0;
    int dismissals = 0;
    std::function<void(bool)> completion;
    auto editor = std::make_unique<PresetMidiEditor>(phrase, [](auto) {});
    auto* content = editor.get();
    MidiPatternEditorWindow window(std::move(editor), phrase,
            [&](auto draft, bool closing, auto complete) {
                REQUIRE(draft.notes[0].pitch == 61);
                REQUIRE_FALSE(closing);
                ++saves;
                completion = complete;
            }, [&] { ++dismissals; });
    window.requestSave(false);
    REQUIRE(saves == 0);
    auto draft = phrase;
    draft.notes[0].pitch = 61;
    window.draftChanged(draft);
    REQUIRE(saves == 0);
    content->keyPressed(KeyPress('S', ModifierKeys::ctrlModifier, 0));
    REQUIRE(saves == 1);
    completion(false);
    REQUIRE(dismissals == 0);
    content->keyPressed(KeyPress('S', ModifierKeys::commandModifier, 0));
    REQUIRE(saves == 2);
    completion(true);
    window.closeButtonPressed();
    REQUIRE(saves == 2);
    REQUIRE(dismissals == 1);
}
