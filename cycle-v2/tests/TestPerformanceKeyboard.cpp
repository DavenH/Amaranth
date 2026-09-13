#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCompiler.h"
#include "Runtime/RealtimeGraphRenderer.h"
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

MouseEvent keyboardMouseEvent(
        Component& component,
        Point<float> position,
        ModifierKeys modifiers) {
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

TEST_CASE("Performance keyboard emits ordinary MIDI and keeps two octaves visible",
        "[cycle-v2][keyboard][midi]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboard keyboard(state, sink);
    keyboard.setBounds(0, 0, 480, 96);

    REQUIRE(keyboard.baseNote() == 48);
    REQUIRE(keyboard.noteLabel(48) == "C2");
    REQUIRE(keyboard.noteLabel(49).isEmpty());
    REQUIRE(keyboard.noteLabel(60) == "C3");
    REQUIRE(keyboard.noteLabel(72) == "C4");
    REQUIRE_FALSE(keyboard.noteBounds(48).isEmpty());
    REQUIRE_FALSE(keyboard.noteBounds(60).isEmpty());
    REQUIRE_FALSE(keyboard.noteBounds(72).isEmpty());
    REQUIRE(keyboard.noteBounds(47).isEmpty());

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

    state.noteOn(1, 48, 0.5f);
    keyboard.shiftOctave(1);

    REQUIRE(keyboard.baseNote() == 60);
    REQUIRE(keyboard.noteLabel(60) == "C3");
    REQUIRE(keyboard.noteLabel(72) == "C4");
    REQUIRE(keyboard.noteLabel(84) == "C5");
    REQUIRE(keyboard.heldNote() == -1);
    REQUIRE_FALSE(keyboard.noteBounds(60).isEmpty());
    REQUIRE_FALSE(keyboard.noteBounds(72).isEmpty());
    REQUIRE_FALSE(keyboard.noteBounds(84).isEmpty());
    REQUIRE(sink.releasedSources.back() == MidiEventSource::PerformanceKeyboard);

    keyboard.shiftOctave(20);
    REQUIRE(keyboard.baseNote() == 103);
    REQUIRE_FALSE(keyboard.noteBounds(127).isEmpty());
}

TEST_CASE("Performance keyboard right click selects preview note without sounding it",
        "[cycle-v2][keyboard][preview-note]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboard keyboard(state, sink);
    keyboard.setBounds(0, 0, 480, 96);
    int selectedNote = -1;
    keyboard.setPreviewNoteSelectedCallback([&selectedNote](int note) {
        selectedNote = note;
    });

    const MouseEvent popup = keyboardMouseEvent(
            keyboard,
            keyboard.noteBounds(60).getCentre(),
            ModifierKeys::rightButtonModifier);
    REQUIRE_FALSE(keyboard.mouseDownOnKey(60, popup));

    REQUIRE(selectedNote == 60);
    REQUIRE(keyboard.heldNote() == -1);
    REQUIRE(sink.messages.empty());
}

TEST_CASE("Performance keyboard panel exposes compact dock interaction targets",
        "[cycle-v2][keyboard][ui]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setBounds(0, 0, 451, 140);

    const Rectangle<float> whiteKey = panel.noteBounds(60);
    const Rectangle<float> blackKey = panel.noteBounds(61);
    const float whiteAspect = whiteKey.getHeight() / whiteKey.getWidth();
    const float blackAspect = blackKey.getHeight() / blackKey.getWidth();
    const Rectangle<float> octaveDown = panel.octaveDownBounds();
    const Rectangle<float> octaveUp = panel.octaveUpBounds();
    const Rectangle<float> play = panel.playButtonBounds();
    const Rectangle<float> progress = panel.progressBounds();

    REQUIRE(octaveDown.getWidth() == 28.f);
    REQUIRE(octaveUp.getWidth() == 28.f);
    REQUIRE(octaveDown.getHeight() == whiteKey.getHeight());
    REQUIRE(octaveUp.getHeight() == whiteKey.getHeight());
    REQUIRE(octaveDown.getRight() < whiteKey.getX());
    REQUIRE(octaveUp.getX() > panel.noteBounds(72).getRight());
    REQUIRE(play.getCentreX() == Catch::Approx(panel.getWidth() * 0.5f).margin(0.5f));
    REQUIRE(progress.getCentreY() == Catch::Approx(play.getCentreY()));
    REQUIRE(progress.getHeight() == play.getHeight());
    REQUIRE(progress.getHeight() == 31.f);
    REQUIRE(progress.getX() == 0.f);
    REQUIRE(progress.getRight() == panel.getWidth());
    REQUIRE(progress.getWidth() > play.getWidth());
    REQUIRE(whiteKey.getWidth() >= 25.f);
    REQUIRE(whiteAspect == Catch::Approx(3.72f));
    REQUIRE(blackAspect == Catch::Approx(3.72f));
    REQUIRE(blackKey.getWidth() < whiteKey.getWidth());
    REQUIRE(blackKey.getHeight() < whiteKey.getHeight());
    REQUIRE_FALSE(panel.noteBounds(72).isEmpty());
    REQUIRE(panel.getLocalBounds().toFloat().contains(whiteKey));
    REQUIRE(panel.getLocalBounds().toFloat().contains(panel.noteBounds(72)));

    panel.setBounds(0, 0, 451, 140);
    const Rectangle<float> compactWhiteKey = panel.noteBounds(48);
    REQUIRE(panel.octaveDownBounds().getHeight() == compactWhiteKey.getHeight());
    REQUIRE(panel.octaveUpBounds().getHeight() == compactWhiteKey.getHeight());
    REQUIRE(compactWhiteKey.getWidth() == 25.f);
    REQUIRE(compactWhiteKey.getHeight() == 93.f);
}

TEST_CASE("Performance keyboard preview transport follows its configured duration",
        "[cycle-v2][keyboard][transport]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setBounds(0, 0, 451, 140);
    panel.setPreviewNote(55);
    panel.setPlaybackDurationSeconds(2.f);

    REQUIRE(panel.startPlayback(1'000.0));
    REQUIRE(panel.isPlaying());
    REQUIRE(panel.previewNote() == 55);
    REQUIRE(panel.playbackProgress() == 0.f);
    REQUIRE(sink.messages.back().isNoteOn());
    REQUIRE(sink.messages.back().getNoteNumber() == 55);

    panel.updatePlayback(2'000.0);
    REQUIRE(panel.isPlaying());
    REQUIRE(panel.playbackProgress() == Catch::Approx(0.5f));

    panel.updatePlayback(3'000.0);
    REQUIRE_FALSE(panel.isPlaying());
    REQUIRE(panel.playbackProgress() == 1.f);
    REQUIRE(sink.messages.back().isNoteOff());
    REQUIRE(sink.messages.back().getNoteNumber() == 55);

    const size_t messageCount = sink.messages.size();
    panel.setPreviewNote(64);
    REQUIRE(panel.previewNote() == 64);
    REQUIRE(sink.messages.size() == messageCount);
}

TEST_CASE("Preview transport duration uses the longest Voice Context",
        "[cycle-v2][keyboard][transport][voice-context]") {
    GraphExecutionPlan plan;
    CompiledVoiceContext shortVoice;
    shortVoice.voiceDurationSeconds = 0.75f;
    CompiledVoiceContext longVoice;
    longVoice.voiceDurationSeconds = 2.5f;
    plan.voiceContexts = { shortVoice, longVoice };

    REQUIRE(RealtimeGraphRenderer::maximumVoiceDurationSeconds(plan)
            == Catch::Approx(2.5f));
    REQUIRE(RealtimeGraphRenderer::maximumVoiceDurationSeconds({}) == 1.f);
}

TEST_CASE("Canvas utilities keep the console clear at the top left",
        "[cycle-v2][canvas][utility-dock][layout]") {
    const Rectangle<float> content { 0.f, 0.f, 1200.f, 700.f };
    const CanvasUtilityDockLayout layout = CanvasUtilityDock::layout(content);

    REQUIRE(layout.minimap.getRight() == content.getRight() - CanvasUtilityDock::margin);
    REQUIRE(layout.legend.getRight() == layout.minimap.getRight());
    REQUIRE(layout.keyboard.getRight() == layout.minimap.getRight());
    REQUIRE(layout.keyboard.getWidth() == 451.f);
    REQUIRE(layout.keyboard.getHeight() == 140.5f);
    REQUIRE(layout.status.getX() == content.getX() + CanvasUtilityDock::margin);
    REQUIRE(layout.status.getY() == content.getY() + CanvasUtilityDock::margin);
    REQUIRE(layout.legend.getY()
            == layout.minimap.getBottom() + CanvasUtilityDock::gap);
    REQUIRE(layout.legend.getHeight()
            == Catch::Approx(CanvasUtilityDock::preferredLegendHeight));
    REQUIRE_FALSE(layout.status.intersects(layout.minimap));
    REQUIRE(content.contains(layout.minimap));
    REQUIRE(content.contains(layout.legend));
    REQUIRE(content.contains(layout.keyboard));
    REQUIRE(content.contains(layout.status));

    const Rectangle<float> compactContent { 0.f, 0.f, 500.f, 300.f };
    const CanvasUtilityDockLayout compact = CanvasUtilityDock::layout(compactContent);
    REQUIRE(compact.keyboard.getWidth() == 451.f);
    REQUIRE(compact.keyboard.getHeight() == 117.f);
    REQUIRE(compact.legend.getHeight() >= CanvasUtilityDock::minimumCompactLegendHeight);
    REQUIRE(compact.minimap.getHeight() == 92.f);
    REQUIRE_FALSE(compact.status.intersects(compact.minimap));
    REQUIRE_FALSE(compact.legend.intersects(compact.keyboard));
    REQUIRE(compactContent.contains(compact.keyboard));
    REQUIRE(compactContent.contains(compact.status));
}
