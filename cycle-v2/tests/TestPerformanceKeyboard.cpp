#include <catch2/catch_test_macros.hpp>

#include "UI/CanvasUtilityDock.h"
#include "UI/PerformanceKeyboard.h"

using namespace CycleV2;

namespace {

class RecordingMidiSink final : public MidiEventSink {
public:
    bool enqueueMidiMessage(const MidiMessage& message, MidiEventSource source) override {
        messages.push_back(message);
        sources.push_back(source);
        return true;
    }

    void releaseMidiSource(MidiEventSource source) override {
        releasedSources.push_back(source);
    }

    std::vector<MidiMessage> messages;
    std::vector<MidiEventSource> sources;
    std::vector<MidiEventSource> releasedSources;
};

}

TEST_CASE("Performance keyboard emits ordinary MIDI and keeps one octave visible",
        "[cycle-v2][keyboard][midi]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboard keyboard(state, sink);
    keyboard.setBounds(0, 0, 480, 96);

    REQUIRE(keyboard.baseNote() == 60);
    REQUIRE(keyboard.noteLabel(60) == "C3");
    REQUIRE(keyboard.noteLabel(61).isEmpty());
    REQUIRE(keyboard.noteLabel(72) == "C4");
    REQUIRE_FALSE(keyboard.noteBounds(60).isEmpty());
    REQUIRE_FALSE(keyboard.noteBounds(72).isEmpty());
    REQUIRE(keyboard.noteBounds(59).isEmpty());

    state.noteOn(1, 60, 0.75f);
    REQUIRE(sink.messages.size() == 1);
    REQUIRE(sink.messages.back().isNoteOn());
    REQUIRE(sink.messages.back().getNoteNumber() == 60);
    REQUIRE(keyboard.heldNote() == 60);

    state.noteOff(1, 60, 0.4f);
    REQUIRE(sink.messages.size() == 2);
    REQUIRE(sink.messages.back().isNoteOff());
    REQUIRE(sink.messages.back().getNoteNumber() == 60);
    REQUIRE(keyboard.heldNote() == -1);
}

TEST_CASE("Performance keyboard octave changes release its owned notes",
        "[cycle-v2][keyboard][midi]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboard keyboard(state, sink);
    keyboard.setBounds(0, 0, 480, 96);

    state.noteOn(1, 60, 0.5f);
    keyboard.shiftOctave(1);

    REQUIRE(keyboard.baseNote() == 72);
    REQUIRE(keyboard.noteLabel(72) == "C4");
    REQUIRE(keyboard.noteLabel(84) == "C5");
    REQUIRE(keyboard.heldNote() == -1);
    REQUIRE_FALSE(keyboard.noteBounds(72).isEmpty());
    REQUIRE_FALSE(keyboard.noteBounds(84).isEmpty());
    REQUIRE(sink.releasedSources.back() == MidiEventSource::PerformanceKeyboard);
}

TEST_CASE("Performance keyboard panel exposes compact dock interaction targets",
        "[cycle-v2][keyboard][ui]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setBounds(0, 0, 360, 108);

    REQUIRE_FALSE(panel.octaveDownBounds().isEmpty());
    REQUIRE_FALSE(panel.octaveUpBounds().isEmpty());
    REQUIRE_FALSE(panel.noteBounds(60).isEmpty());
    REQUIRE_FALSE(panel.noteBounds(72).isEmpty());
    REQUIRE(panel.getLocalBounds().toFloat().contains(panel.noteBounds(60)));
    REQUIRE(panel.getLocalBounds().toFloat().contains(panel.noteBounds(72)));
}

TEST_CASE("Canvas utilities share one right-aligned dock layout",
        "[cycle-v2][canvas][utility-dock][layout]") {
    const Rectangle<float> content { 0.f, 0.f, 1200.f, 700.f };
    const CanvasUtilityDockLayout layout = CanvasUtilityDock::layout(content);

    REQUIRE(layout.minimap.getRight() == content.getRight() - CanvasUtilityDock::margin);
    REQUIRE(layout.legend.getRight() == layout.minimap.getRight());
    REQUIRE(layout.keyboard.getRight() == layout.minimap.getRight());
    REQUIRE(layout.status.getX() == content.getX() + CanvasUtilityDock::margin);
    REQUIRE(layout.legend.getY()
            == layout.minimap.getBottom() + CanvasUtilityDock::gap);
    REQUIRE_FALSE(layout.status.intersects(layout.keyboard));
    REQUIRE(content.contains(layout.minimap));
    REQUIRE(content.contains(layout.legend));
    REQUIRE(content.contains(layout.keyboard));
    REQUIRE(content.contains(layout.status));

    const Rectangle<float> compactContent { 0.f, 0.f, 500.f, 300.f };
    const CanvasUtilityDockLayout compact = CanvasUtilityDock::layout(compactContent);
    REQUIRE_FALSE(compact.status.intersects(compact.keyboard));
    REQUIRE_FALSE(compact.legend.intersects(compact.keyboard));
    REQUIRE(compactContent.contains(compact.keyboard));
    REQUIRE(compactContent.contains(compact.status));
}
