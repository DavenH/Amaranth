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

TEST_CASE("Performance keyboard keeps a loaded preview note visible",
        "[cycle-v2][keyboard][preview-note][preset]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setBounds(0, 0, 489, 140);

    panel.setPreviewNote(73);

    REQUIRE(panel.previewNote() == 73);
    REQUIRE(panel.baseNote() == 60);
    REQUIRE_FALSE(panel.noteBounds(73).isEmpty());
    REQUIRE(sink.messages.empty());
}

TEST_CASE("Performance keyboard panel exposes compact dock interaction targets",
        "[cycle-v2][keyboard][ui]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setBounds(0, 0, 489, 140);

    const Rectangle<float> whiteKey = panel.noteBounds(60);
    const Rectangle<float> blackKey = panel.noteBounds(61);
    const float whiteAspect = whiteKey.getHeight() / whiteKey.getWidth();
    const float blackAspect = blackKey.getHeight() / blackKey.getWidth();
    const Rectangle<float> octaveDown = panel.octaveDownBounds();
    const Rectangle<float> octaveUp = panel.octaveUpBounds();
    const Rectangle<float> modWheel = panel.modWheelBounds();
    const Rectangle<float> play = panel.playButtonBounds();
    const Rectangle<float> progress = panel.progressBounds();

    REQUIRE(octaveDown.getWidth() == 28.f);
    REQUIRE(octaveUp.getWidth() == 28.f);
    REQUIRE(modWheel.getWidth() == 32.f);
    REQUIRE(modWheel.getHeight() == whiteKey.getHeight());
    REQUIRE(modWheel.getRight() < octaveDown.getX());
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

    panel.setBounds(0, 0, 464, 117);
    const Rectangle<float> compactWhiteKey = panel.noteBounds(48);
    REQUIRE(panel.modWheelBounds().getWidth() == 24.f);
    REQUIRE(panel.modWheelBounds().getHeight() == compactWhiteKey.getHeight());
    REQUIRE(panel.octaveDownBounds().getHeight() == compactWhiteKey.getHeight());
    REQUIRE(panel.octaveUpBounds().getHeight() == compactWhiteKey.getHeight());
    REQUIRE(compactWhiteKey.getWidth() == 25.f);
    REQUIRE(compactWhiteKey.getHeight() == 76.f);
}

TEST_CASE("Performance mod wheel drag controls preview CC 1 and audition start",
        "[cycle-v2][keyboard][mod-wheel][transport]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setBounds(0, 0, 489, 140);
    std::vector<int> previewValues;
    int gestureStarts = 0;
    int gestureEnds = 0;
    panel.setModWheelValueChangedCallback([&previewValues](int value) {
        previewValues.push_back(value);
    });
    panel.setModWheelGestureStartedCallback([&gestureStarts] {
        ++gestureStarts;
    });
    panel.setModWheelGestureEndedCallback([&gestureEnds] {
        ++gestureEnds;
    });

    Component* wheel = nullptr;
    for (int i = 0; i < panel.getNumChildComponents(); ++i) {
        Component* child = panel.getChildComponent(i);
        if (child->getName() == "PerformanceKeyboard.ModWheel") {
            wheel = child;
            break;
        }
    }
    REQUIRE(wheel != nullptr);
    REQUIRE(wheel->getName() == "PerformanceKeyboard.ModWheel");

    wheel->mouseDown(keyboardMouseEvent(
            *wheel,
            { wheel->getWidth() * 0.5f, wheel->getHeight() * 0.5f },
            ModifierKeys::leftButtonModifier));
    wheel->mouseDrag(keyboardMouseEvent(
            *wheel,
            { wheel->getWidth() * 0.5f, 0.f },
            ModifierKeys::leftButtonModifier));
    wheel->mouseUp(keyboardMouseEvent(
            *wheel,
            { wheel->getWidth() * 0.5f, 0.f },
            {}));

    REQUIRE(panel.modWheelValue() == 127);
    REQUIRE(previewValues.size() == 2);
    REQUIRE(previewValues.front() > 0);
    REQUIRE_FALSE(sink.messages.empty());
    REQUIRE(sink.messages.back().isController());
    REQUIRE(sink.messages.back().getControllerNumber() == 1);
    REQUIRE(sink.messages.back().getControllerValue() == 127);
    REQUIRE(previewValues.back() == 127);
    REQUIRE(gestureStarts == 1);
    REQUIRE(gestureEnds == 1);

    REQUIRE(wheel->keyPressed(KeyPress(KeyPress::downKey)));
    REQUIRE(panel.modWheelValue() == 126);
    REQUIRE(sink.messages.back().getControllerValue() == 126);
    REQUIRE(previewValues.back() == 126);

    panel.setPreviewNote(55);
    sink.messages.clear();
    REQUIRE(panel.startPlayback(1'000.0));
    REQUIRE(sink.messages.size() == 2);
    REQUIRE(sink.messages[0].isController());
    REQUIRE(sink.messages[0].getControllerNumber() == 1);
    REQUIRE(sink.messages[0].getControllerValue() == 126);
    REQUIRE(sink.messages[1].isNoteOn());
    REQUIRE(sink.messages[1].getNoteNumber() == 55);

    panel.stopPlayback();
    REQUIRE(sink.messages.back().isNoteOff());
    REQUIRE(sink.messages.back().getNoteNumber() == 55);
}

TEST_CASE("Performance keyboard preview transport follows its configured duration",
        "[cycle-v2][keyboard][transport]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setBounds(0, 0, 489, 140);
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
    REQUIRE(layout.keyboard.getCentreX() == content.getCentreX());
    REQUIRE(layout.keyboard.getY() == content.getY());
    REQUIRE(layout.keyboard.getWidth() == 489.f);
    REQUIRE(layout.keyboard.getHeight() == 140.5f);
    REQUIRE(layout.status.getX() == content.getX() + CanvasUtilityDock::margin);
    REQUIRE(layout.status.getY() == content.getY() + CanvasUtilityDock::margin);
    REQUIRE(layout.legend.getY()
            == layout.minimap.getBottom() + CanvasUtilityDock::gap);
    REQUIRE(layout.legend.getHeight()
            == Catch::Approx(CanvasUtilityDock::preferredLegendHeight));
    REQUIRE_FALSE(layout.status.intersects(layout.minimap));
    REQUIRE_FALSE(layout.status.intersects(layout.keyboard));
    REQUIRE_FALSE(layout.minimap.intersects(layout.keyboard));
    REQUIRE_FALSE(layout.legend.intersects(layout.keyboard));
    REQUIRE(content.contains(layout.minimap));
    REQUIRE(content.contains(layout.legend));
    REQUIRE(content.contains(layout.keyboard));
    REQUIRE(content.contains(layout.status));

    const Rectangle<float> compactContent { 0.f, 0.f, 500.f, 300.f };
    const CanvasUtilityDockLayout compact = CanvasUtilityDock::layout(compactContent);
    REQUIRE(compact.keyboard.getWidth() == 464.f);
    REQUIRE(compact.keyboard.getHeight() == CanvasUtilityDock::preferredKeyboardHeight);
    REQUIRE(compact.keyboard.getY() == compactContent.getY());
    REQUIRE(compact.keyboard.getCentreX() == compactContent.getCentreX());
    REQUIRE(compact.legend.isEmpty());
    REQUIRE(compact.minimap.getHeight() == 50.f);
    REQUIRE_FALSE(compact.status.intersects(compact.minimap));
    REQUIRE_FALSE(compact.legend.intersects(compact.keyboard));
    REQUIRE(compactContent.contains(compact.keyboard));
    REQUIRE(compactContent.contains(compact.status));
}
