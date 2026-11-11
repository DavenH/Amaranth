#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "Graph/GraphCompiler.h"
#include "Runtime/RealtimeGraphRenderer.h"
#include "UI/CanvasUtilityDock.h"
#include "UI/PerformanceKeyboard.h"
#include "UI/PresetMidiEditor.h"

using namespace CycleV2;

namespace {

class RecordingMidiSink final : public MidiEventSink {
public:
    bool enqueueMidiMessage(const MidiMessage& message, MidiEventSource source) override {
        messages.push_back(message);
        sources.push_back(source);
        timestamps.push_back(-1.0);
        return true;
    }

    bool enqueueMidiMessageAt(
            const MidiMessage& message,
            MidiEventSource source,
            double timestampSeconds) override {
        messages.push_back(message);
        sources.push_back(source);
        timestamps.push_back(timestampSeconds);
        return true;
    }

    void releaseMidiSource(MidiEventSource source) override {
        releasedSources.push_back(source);
    }

    std::vector<MidiMessage> messages;
    std::vector<MidiEventSource> sources;
    std::vector<double> timestamps;
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

Component* namedControl(Component& parent, const String& name) {
    if (parent.getName() == name) {
        return &parent;
    }
    for (auto* child : parent.getChildren()) {
        if (auto* match = namedControl(*child, name)) {
            return match;
        }
    }
    return nullptr;
}

}

TEST_CASE("Performance keyboard emits ordinary MIDI and keeps three octaves visible",
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
    REQUIRE_FALSE(keyboard.noteBounds(84).isEmpty());
    REQUIRE(keyboard.noteLabel(84) == "C5");
    REQUIRE(keyboard.noteBounds(85).isEmpty());
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

    state.noteOn(1, 84, 0.75f);
    REQUIRE(sink.messages.back().isNoteOn());
    REQUIRE(sink.messages.back().getNoteNumber() == 84);
    REQUIRE(keyboard.heldNote() == 84);
    state.noteOff(1, 84, 0.f);
    REQUIRE(sink.messages.back().isNoteOff());
    REQUIRE(sink.messages.back().getNoteNumber() == 84);
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
    REQUIRE(keyboard.baseNote() == 91);
    REQUIRE_FALSE(keyboard.noteBounds(127).isEmpty());

    keyboard.revealRange(24, 96);
    REQUIRE(keyboard.baseNote() == 36);
    REQUIRE_FALSE(keyboard.noteBounds(60).isEmpty());
    keyboard.revealRange(20, 42);
    REQUIRE(keyboard.baseNote() == 12);
    REQUIRE_FALSE(keyboard.noteBounds(42).isEmpty());
}

TEST_CASE("Dark keyboard edges do not expose bright background pixels",
        "[cycle-v2][keyboard][keyboard-pixels]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboard keyboard(state, sink);
    for (const int width : { 210, 220, 359, 360, 361, 527, 528, 529 }) {
        keyboard.setSize(width, 50);
        for (const float scale : { 1.f, 1.25f, 1.5f, 2.f }) {
            const auto image = keyboard.createComponentSnapshot(keyboard.getLocalBounds(), true, scale);
            int brightPixels = 0;
            for (int y = 0; y < roundToInt(8.f * scale); ++y) {
                for (int x = 0; x < image.getWidth(); ++x) {
                    const auto colour = image.getPixelAt(x, y);
                    brightPixels += colour.getBrightness() > 0.65f ? 1 : 0;
                }
            }
            INFO("width=" << width << " scale=" << scale);
            REQUIRE(brightPixels == 0);
        }
    }
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
    panel.setBounds(0, 0, 489, 112);

    panel.setPreviewNote(85);

    REQUIRE(panel.previewNote() == 85);
    REQUIRE(panel.baseNote() == 60);
    REQUIRE_FALSE(panel.noteBounds(85).isEmpty());
    REQUIRE(sink.messages.empty());
}

TEST_CASE("Preset phrase playback sends notes and CC through the performance MIDI path",
        "[cycle-v2][keyboard][sequence][transport]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setBounds(0, 0, 489, 135);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 2.0;
    phrase.notes.push_back({ 67, 110, 0.0, 0.5 });
    phrase.notes.push_back({ 72, 90, 0.5, 0.5 });
    phrase.controls.push_back({ 1, 15, 0.0 });
    phrase.controls.push_back({ 1, 115, 1.0 });
    panel.setSequence(phrase);

    REQUIRE_FALSE(panel.noteBounds(67).isEmpty());
    REQUIRE_FALSE(panel.noteBounds(72).isEmpty());
    const double startedAt = Time::getMillisecondCounterHiRes();
    REQUIRE(panel.startPlayback(startedAt));
    const auto noteEventIndex = [&](int note, bool on) {
        for (size_t index = 0; index < sink.messages.size(); ++index) {
            const auto& message = sink.messages[index];
            if ((on ? message.isNoteOn() : message.isNoteOff())
                    && message.getNoteNumber() == note) {
                return index;
            }
        }
        return sink.messages.size();
    };
    const size_t firstOn = noteEventIndex(67, true);
    const size_t firstOff = noteEventIndex(67, false);
    const size_t secondOn = noteEventIndex(72, true);
    REQUIRE(firstOn < sink.messages.size());
    REQUIRE(firstOff < sink.messages.size());
    REQUIRE(secondOn < sink.messages.size());
    REQUIRE(sink.sources[firstOn] == MidiEventSource::PatternPlayback);
    REQUIRE(sink.sources[firstOff] == MidiEventSource::PatternPlayback);
    REQUIRE(sink.sources[secondOn] == MidiEventSource::PatternPlayback);
    REQUIRE(sink.timestamps[firstOn] == Catch::Approx(startedAt / 1000.0));
    REQUIRE(sink.timestamps[firstOff] == Catch::Approx(startedAt / 1000.0 + 0.5));
    REQUIRE(sink.timestamps[secondOn] == Catch::Approx(startedAt / 1000.0 + 0.5));
    const size_t scheduledCount = sink.messages.size();
    REQUIRE(scheduledCount > 30);
    panel.updatePlayback(startedAt + 800.0);
    REQUIRE(sink.messages.size() > scheduledCount);
    const auto matching = std::find_if(sink.messages.begin(), sink.messages.end(),
            [](const MidiMessage& message) {
                return message.isController()
                        && message.getControllerValue() == 95;
            });
    REQUIRE(matching != sink.messages.end());
    const size_t matchingIndex = (size_t) std::distance(sink.messages.begin(), matching);
    REQUIRE(sink.timestamps[matchingIndex]
            == Catch::Approx(startedAt / 1000.0 + 0.8));
    panel.stopPlayback();
    REQUIRE(sink.releasedSources.back() == MidiEventSource::PatternPlayback);
}

TEST_CASE("Preset automation interpolates the configured MIDI controller",
        "[cycle-v2][keyboard][sequence][transport]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 2.0;
    phrase.notes.push_back({ 60, 100, 0.0, 1.5 });
    phrase.controls.push_back({ 3, 10, 0.0 });
    phrase.controls.push_back({ 3, 110, 1.0 });
    panel.setSequence(phrase);

    const double startedAt = Time::getMillisecondCounterHiRes();
    REQUIRE(panel.startPlayback(startedAt));
    const auto initialCount = sink.messages.size();
    REQUIRE(initialCount > 40);
    const auto atHalfSecond = std::find_if(sink.messages.begin(), sink.messages.end(),
            [](const MidiMessage& message) {
                return message.isController()
                        && message.getControllerNumber() == 3
                        && message.getControllerValue() == 60;
            });
    REQUIRE(atHalfSecond != sink.messages.end());
    const size_t index = (size_t) std::distance(sink.messages.begin(), atHalfSecond);
    REQUIRE(sink.timestamps[index] == Catch::Approx(startedAt / 1000.0 + 0.5));
    panel.updatePlayback(startedAt + 500.0);
    REQUIRE(sink.messages.size() > initialCount);
}

TEST_CASE("Every automation lane sets its first value before the first note",
        "[cycle-v2][keyboard][sequence][automation][timing]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 2.0;
    phrase.notes.push_back({ 60, 100, 0.0, 1.0 });
    phrase.controls.push_back({ 1, 20, 0.25 });
    phrase.controls.push_back({ 1, 100, 1.0 });
    phrase.controls.push_back({ 73, 40, 0.25 });
    phrase.controls.push_back({ 73, 120, 1.0 });
    panel.setSequence(phrase);
    panel.setMappedControllers({1, 73});

    REQUIRE(panel.startPlayback(1000.0));
    // A wall-clock seed sent after this origin would overwrite the score's
    // initial CC after note-on, potentially until the next changed CC value.
    REQUIRE(sink.sources[0] == MidiEventSource::PerformanceKeyboard);
    REQUIRE(sink.sources[1] == MidiEventSource::PerformanceKeyboard);
    REQUIRE(sink.timestamps[0] == 1.0);
    REQUIRE(sink.timestamps[1] == 1.0);
    const auto firstNote = std::find_if(sink.messages.begin(), sink.messages.end(),
            [](const MidiMessage& message) { return message.isNoteOn(); });
    REQUIRE(firstNote != sink.messages.end());
    const size_t noteIndex = (size_t) std::distance(sink.messages.begin(), firstNote);
    REQUIRE(sink.timestamps[noteIndex] == Catch::Approx(1.0));

    const auto hasInitialControl = [&](int controller, int value) {
        for (size_t index = 0; index < noteIndex; ++index) {
            const auto& message = sink.messages[index];
            if (sink.sources[index] == MidiEventSource::PatternPlayback
                    && message.isController()
                    && message.getControllerNumber() == controller
                    && message.getControllerValue() == value
                    && sink.timestamps[index] == Catch::Approx(1.0)) {
                return true;
            }
        }
        return false;
    };
    REQUIRE(hasInitialControl(1, 20));
    REQUIRE(hasInitialControl(73, 40));

    const auto laterCc73 = std::find_if(sink.messages.begin(), sink.messages.end(),
            [](const MidiMessage& message) {
                return message.isController()
                        && message.getControllerNumber() == 73
                        && message.getControllerValue() > 40;
            });
    REQUIRE(laterCc73 != sink.messages.end());
    panel.stopPlayback();
}

TEST_CASE("Pattern automation schedules dense timestamped values ahead of UI ticks",
        "[cycle-v2][keyboard][sequence][automation][timing]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 2.0;
    phrase.notes.push_back({ 60, 100, 0.0, 1.5 });
    phrase.controls.push_back({ 1, 0, 0.0 });
    phrase.controls.push_back({ 1, 127, 1.0 });
    panel.setSequence(phrase);

    REQUIRE(panel.startPlayback(1000.0));
    std::vector<double> controlTimes;
    for (size_t index = 0; index < sink.messages.size(); ++index) {
        if (sink.messages[index].isController()
                && sink.sources[index] == MidiEventSource::PatternPlayback) {
            REQUIRE(sink.timestamps[index] >= 1.0);
            REQUIRE(sink.timestamps[index] <= 1.5);
            controlTimes.push_back(sink.timestamps[index]);
        }
    }
    REQUIRE(controlTimes.size() >= 50);
    for (size_t index = 1; index < controlTimes.size(); ++index) {
        REQUIRE(controlTimes[index] - controlTimes[index - 1] <= 0.011);
    }
    panel.stopPlayback();
    REQUIRE(sink.releasedSources.back() == MidiEventSource::PatternPlayback);
}

TEST_CASE("Sequence notes retain fractional timing when the UI transport updates late",
        "[cycle-v2][keyboard][sequence][transport]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 1.0;
    phrase.notes.push_back({ 48, 100, 0.137, 0.123 });
    phrase.notes.push_back({ 50, 80, 0.319, 0.157 });
    panel.setSequence(phrase);

    REQUIRE(panel.startPlayback(1000.0));
    REQUIRE(sink.timestamps[1] == Catch::Approx(1.137));
    REQUIRE(sink.timestamps[2] == Catch::Approx(1.260));
    REQUIRE(sink.timestamps[3] == Catch::Approx(1.319));
    REQUIRE(sink.timestamps[4] == Catch::Approx(1.476));
    REQUIRE(sink.sources[1] == MidiEventSource::PatternPlayback);
    const size_t eventCount = sink.messages.size();

    panel.updatePlayback(1700.0);
    REQUIRE(sink.messages.size() == eventCount);
    panel.stopPlayback();
    REQUIRE(sink.releasedSources.back() == MidiEventSource::PatternPlayback);
}

TEST_CASE("Preview transport toggle stops an active sequence source",
        "[cycle-v2][keyboard][sequence][transport][stop]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes.push_back({ 60, 100, 0.0, 3.0 });
    panel.setSequence(phrase);

    panel.togglePlayback();
    REQUIRE(panel.isPlaying());
    panel.togglePlayback();
    REQUIRE_FALSE(panel.isPlaying());
    REQUIRE(sink.releasedSources.back() == MidiEventSource::PatternPlayback);
}

TEST_CASE("Record button captures MIDI note and controller gestures into the preset phrase",
        "[cycle-v2][keyboard][sequence][record]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setBounds(0, 0, 489, 135);
    PresetMidiSequence saved;
    int saveCount = 0;
    panel.setSequenceChangedCallback([&](PresetMidiSequence sequence) {
        saved = std::move(sequence);
        ++saveCount;
    });
    Button* record = nullptr;
    for (int index = 0; index < panel.getNumChildComponents(); ++index) {
        if (auto* button = dynamic_cast<Button*>(panel.getChildComponent(index))) {
            if (button->getName() == "PerformanceKeyboard.RecordSequence") {
                record = button;
            }
        }
    }
    REQUIRE(record != nullptr);
    record->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(panel.isRecording());
    const double now = Time::getMillisecondCounterHiRes() / 1000.0;
    panel.recordMidiMessage(MidiMessage::noteOn(1, 72, (uint8) 101), now + 0.1);
    panel.recordMidiMessage(MidiMessage::controllerEvent(1, 1, 75), now + 0.2);
    panel.recordMidiMessage(MidiMessage::noteOff(1, 72), now + 0.6);
    record->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);

    REQUIRE_FALSE(panel.isRecording());
    REQUIRE(saveCount == 1);
    REQUIRE(saved.notes.size() == 1);
    REQUIRE(saved.notes[0].pitch == 72);
    REQUIRE(panel.baseNote() == 48);
    REQUIRE_FALSE(panel.noteBounds(72).isEmpty());
    REQUIRE(saved.notes[0].velocity == 101);
    REQUIRE(saved.notes[0].durationSeconds == Catch::Approx(0.5).margin(0.01));
    REQUIRE(saved.controls.size() == 1);
    REQUIRE(saved.controls[0].controller == 1);
    REQUIRE(saved.controls[0].value == 75);
}

TEST_CASE("Performance keyboard panel exposes compact dock interaction targets",
        "[cycle-v2][keyboard][ui]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setBounds(0, 0,
            640,
            (int) CanvasUtilityDock::preferredKeyboardHeight);

    const auto checkLayout = [&] {
        const auto first = panel.noteBounds(panel.baseNote());
        const auto last = panel.noteBounds(panel.highestNote());
        const auto actions = panel.playBounds().getUnion(panel.editBounds());
        REQUIRE(actions.getX() == 18.f);
        REQUIRE(panel.playBounds().getY() == panel.recordBounds().getY());
        REQUIRE(panel.recordBounds().getY() == panel.editBounds().getY());
        REQUIRE(panel.playBounds().getRight() < panel.recordBounds().getX());
        REQUIRE(panel.recordBounds().getRight() < panel.editBounds().getX());
        REQUIRE(panel.editBounds().getRight() < panel.modWheelBounds().getX());
        REQUIRE(panel.octaveDownBounds().getRight() < first.getX());
        REQUIRE(first.getX() == panel.octaveDownBounds().getRight() + 2.f);
        REQUIRE(panel.octaveUpBounds().getX() > last.getRight());
        REQUIRE(panel.octaveUpBounds().getX() == last.getRight() + 2.f);
        REQUIRE(panel.modWheelBounds().getRight() < panel.octaveDownBounds().getX());
        REQUIRE(first.getY() == 3.f);
        REQUIRE(first.getBottom() == panel.getHeight() - 3.f);
        REQUIRE(panel.noteBounds(panel.baseNote() + 1).getWidth() < first.getWidth());
        REQUIRE(panel.noteBounds(panel.baseNote() + 1).getHeight() < first.getHeight());
        REQUIRE(panel.getLocalBounds().toFloat().contains(last));
        REQUIRE(panel.getLocalBounds().toFloat().contains(panel.modWheelBounds()));
    };
    panel.setSize(1200, 56);
    checkLayout();
    REQUIRE(panel.noteBounds(60).getWidth() == Catch::Approx(24.f));
    REQUIRE(panel.highestNote() - panel.baseNote() == 36);
    panel.setSize(500, 56);
    checkLayout();
    REQUIRE(panel.noteBounds(60).getWidth() >= 10.f);

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

    Component* wheel = namedControl(panel, "PerformanceKeyboard.ModWheel");
    REQUIRE(wheel != nullptr);
    REQUIRE(wheel->getName() == "PerformanceKeyboard.ModWheel");

    wheel->mouseDown(keyboardMouseEvent(
            *wheel,
            { wheel->getWidth() * 0.5f, wheel->getHeight() * 0.5f },
            ModifierKeys::leftButtonModifier));
    REQUIRE(panel.modWheelValue() == 0);
    wheel->mouseDrag(keyboardMouseEvent(*wheel,
            { wheel->getWidth() * 0.5f, -128.f }, ModifierKeys::leftButtonModifier));
    wheel->mouseDrag(keyboardMouseEvent(
            *wheel,
            { wheel->getWidth() * 0.5f, -254.f },
            ModifierKeys::leftButtonModifier));
    wheel->mouseUp(keyboardMouseEvent(
            *wheel,
            { wheel->getWidth() * 0.5f, -254.f },
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

    REQUIRE(wheel->keyPressed(KeyPress(KeyPress::homeKey)));
    REQUIRE(panel.modWheelValue() == 0);
    REQUIRE(wheel->keyPressed(KeyPress(KeyPress::endKey)));
    REQUIRE(panel.modWheelValue() == 127);
    REQUIRE(wheel->keyPressed(KeyPress(KeyPress::downKey)));
    REQUIRE(panel.modWheelValue() == 126);

    panel.setModWheelValue(40);
    wheel->mouseDown(keyboardMouseEvent(*wheel, { 22.f, 24.f },
            ModifierKeys::leftButtonModifier | ModifierKeys::shiftModifier));
    wheel->mouseDrag(keyboardMouseEvent(*wheel, { 22.f, -56.f },
            ModifierKeys::leftButtonModifier | ModifierKeys::shiftModifier));
    wheel->mouseUp(keyboardMouseEvent(*wheel, { 22.f, -56.f }, {}));
    REQUIRE(panel.modWheelValue() == 44);
    panel.setModWheelValue(126);

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

TEST_CASE("Mapped CC wheels precede the keyboard and send live MIDI",
        "[cycle-v2][keyboard][midi-cc][ui]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setMappedControllers({ 1, 73 });
    panel.setBounds(0, 0,
            640 + 38,
            (int) CanvasUtilityDock::preferredKeyboardHeight);

    const auto ccBounds = panel.controllerBounds(73);
    REQUIRE(ccBounds.getWidth() == 44.f);
    REQUIRE(ccBounds.getX() == panel.modWheelBounds().getRight());
    REQUIRE(panel.modWheelBounds().getRight() < panel.octaveDownBounds().getX());
    REQUIRE(panel.noteBounds(60).getWidth() >= 10.f);

    Component* ccWheel = namedControl(panel, "PerformanceKeyboard.CC73");
    REQUIRE(ccWheel != nullptr);
    ccWheel->mouseDown(keyboardMouseEvent(*ccWheel,
            { ccWheel->getWidth() * 0.5f, 0.f },
            ModifierKeys::leftButtonModifier));
    ccWheel->mouseDrag(keyboardMouseEvent(*ccWheel,
            { ccWheel->getWidth() * 0.5f, -254.f }, ModifierKeys::leftButtonModifier));
    ccWheel->mouseUp(keyboardMouseEvent(*ccWheel,
            { ccWheel->getWidth() * 0.5f, 0.f }, {}));
    REQUIRE(panel.controllerValue(73) == 127);
    REQUIRE(sink.messages.back().isController());
    REQUIRE(sink.messages.back().getControllerNumber() == 73);
    REQUIRE(sink.messages.back().getControllerValue() == 127);

    sink.messages.clear();
    REQUIRE(panel.startPlayback(1000.0));
    REQUIRE(sink.messages.size() == 3);
    REQUIRE(sink.messages[0].getControllerNumber() == 1);
    REQUIRE(sink.messages[1].getControllerNumber() == 73);
    REQUIRE(sink.messages[1].getControllerValue() == 127);
    REQUIRE(sink.messages[2].isNoteOn());
    panel.stopPlayback();

    const auto compactDock = CanvasUtilityDock::layout(
            { 0.f, 0.f, 500.f, 500.f });
    panel.setBounds(compactDock.keyboard.toNearestInt());
    REQUIRE(panel.noteBounds(60).getWidth() >= 10.f);
    auto* dial = namedControl(panel, "PerformanceKeyboard.CC73");
    REQUIRE(dial != nullptr);
    auto* viewport = dial->findParentComponentOfClass<Viewport>();
    REQUIRE(viewport != nullptr);
    viewport->setViewPosition(44, 0);
    REQUIRE(panel.getLocalBounds().toFloat().contains(panel.controllerBounds(73)));
    panel.setMappedControllers({ 1, 11, 14, 15, 16, 17, 18, 19, 73, 120 });
    dial = namedControl(panel, "PerformanceKeyboard.CC120");
    REQUIRE(dial != nullptr);
    viewport = dial->findParentComponentOfClass<Viewport>();
    viewport->setViewPosition(10000, 0);
    REQUIRE(panel.getLocalBounds().toFloat().contains(panel.controllerBounds(120)));
    REQUIRE(dial->keyPressed(KeyPress(KeyPress::endKey)));
    REQUIRE(sink.messages.back().getControllerNumber() == 120);
    REQUIRE(sink.messages.back().getControllerValue() == 127);

    panel.setMappedControllers({ 1 });
    REQUIRE(viewport->getViewPositionX() == 0);
    REQUIRE(panel.getLocalBounds().toFloat().contains(panel.modWheelBounds()));
}

TEST_CASE("Performance waveform stays fixed across controller mapping counts",
        "[cycle-v2][keyboard][performance-layout]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    for (const int width : { 1200, 1438 }) {
        panel.setSize(width, 56);
        const auto waveformBounds = panel.waveformPreview().getBounds();
        const auto keyBounds = panel.noteBounds(panel.baseNote());
        for (const std::vector<int> mappings : {
                std::vector<int> {}, { 1, 73 }, { 1, 11, 14, 73, 120 },
                { 1, 11, 14, 15, 16, 17, 18, 19, 73, 120 } }) {
            panel.setMappedControllers(mappings);
            INFO("width=" << width << " mappings=" << mappings.size());
            REQUIRE(panel.waveformPreview().getBounds() == waveformBounds);
            REQUIRE(panel.noteBounds(panel.baseNote()) == keyBounds);
            auto* mod = namedControl(panel, "PerformanceKeyboard.ModWheel");
            REQUIRE(mod != nullptr);
            auto* viewport = mod->findParentComponentOfClass<Viewport>();
            REQUIRE(viewport != nullptr);
            REQUIRE(viewport->getWidth() == 5 * 44);
            REQUIRE(viewport->getRight() + 12 == panel.octaveDownBounds().getX());
        }
    }
    for (const int width : { 1200, 1199, 1100, 1000, 950, 900 }) {
        panel.setSize(width, 56);
        const auto& waveform = panel.waveformPreview();
        if (waveform.isVisible()) {
            REQUIRE(waveform.getWidth() <= 384);
            REQUIRE(768 % waveform.getWidth() == 0);
            REQUIRE(waveform.getRight() == panel.getWidth() - 18);
            REQUIRE(waveform.getX() == panel.octaveUpBounds().getRight() + 18);
        }
    }
}

TEST_CASE("Unmapped controller slots are visible and cannot emit MIDI",
        "[cycle-v2][keyboard][performance-layout]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setSize(1438, 56);
    panel.setVisible(true);
    auto* mod = namedControl(panel, "PerformanceKeyboard.ModWheel");
    REQUIRE(mod != nullptr);
    auto* viewport = mod->findParentComponentOfClass<Viewport>();
    REQUIRE(viewport != nullptr);
    auto* strip = viewport->getViewedComponent();
    const auto image = strip->createComponentSnapshot(strip->getLocalBounds());
    for (int slot = 1; slot < 5; ++slot) {
        const auto rim = image.getPixelAt(slot * 44 + 9, 15);
        REQUIRE(rim.getAlpha() > 0);
        REQUIRE(rim.getAlpha() < 255);
        const auto point = panel.getLocalPoint(strip, Point<int>(slot * 44 + 22, 15));
        auto* target = panel.getComponentAt(point);
        REQUIRE(dynamic_cast<Slider*>(target) == nullptr);
        if (target != nullptr) {
            const auto local = target->getLocalPoint(&panel, point).toFloat();
            target->mouseDown(keyboardMouseEvent(*target, local, ModifierKeys::leftButtonModifier));
            target->mouseUp(keyboardMouseEvent(*target, local, {}));
        }
    }
    REQUIRE(sink.messages.empty());
    panel.setMappedControllers({ 1, 73 });
    REQUIRE(panel.controllerBounds(73).getWidth() == 44.f);
    REQUIRE(panel.controllerBounds(73).getX() == panel.modWheelBounds().getRight());
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

TEST_CASE("Preset playback lasts for the roll length after its last note ends",
        "[cycle-v2][keyboard][sequence][transport]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes.push_back({ 60, 100, 0.0, 0.5 });
    panel.setSequence(phrase);

    REQUIRE(panel.startPlayback(1'000.0));
    panel.updatePlayback(2'000.0);
    REQUIRE(panel.isPlaying());
    REQUIRE(panel.playbackProgress() == Catch::Approx(0.25f));
    REQUIRE(sink.messages.back().isNoteOff());
    panel.updatePlayback(5'000.0);
    REQUIRE_FALSE(panel.isPlaying());
    REQUIRE(panel.playbackProgress() == 1.f);
}

TEST_CASE("A roll without notes auditions the selected key for audition length",
        "[cycle-v2][keyboard][sequence][transport]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setPreviewNote(55);
    panel.setPlaybackDurationSeconds(2.f);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 10.0;
    phrase.controls.push_back({ 1, 20, 0.0 });
    panel.setSequence(phrase);

    REQUIRE(panel.startPlayback(1'000.0));
    REQUIRE(std::any_of(sink.messages.begin(), sink.messages.end(),
            [](const MidiMessage& message) {
                return message.isController()
                        && message.getControllerNumber() == 1
                        && message.getControllerValue() == 20;
            }));
    REQUIRE(panel.heldNote() == 55);
    panel.updatePlayback(2'000.0);
    REQUIRE(panel.isPlaying());
    REQUIRE(panel.playbackProgress() == Catch::Approx(0.5f));
    panel.updatePlayback(3'000.0);
    REQUIRE_FALSE(panel.isPlaying());
    REQUIRE(panel.playbackProgress() == 1.f);
    REQUIRE(sink.messages.back().isNoteOff());
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

TEST_CASE("Canvas utilities share a single-line footer with a right-aligned legend",
        "[cycle-v2][canvas][utility-dock][layout]") {
    for (const float width : { 500.f, 1200.f }) {
        const Rectangle<float> content(0.f, 0.f, width, 700.f);
        const auto layout = CanvasUtilityDock::layout(content);
        REQUIRE(layout.keyboard.getWidth() == width);
        REQUIRE(layout.keyboard.getHeight() == CanvasUtilityDock::preferredKeyboardHeight);
        REQUIRE(layout.keyboard.getY() == content.getY());
        REQUIRE(layout.footer.getBottom() == content.getBottom());
        REQUIRE(layout.footer.getHeight() == 30.f);
        REQUIRE(layout.status.getX() == content.getX() + CanvasUtilityDock::margin);
        REQUIRE(layout.legend.getRight() == content.getRight() - CanvasUtilityDock::margin);
        REQUIRE(layout.status.getRight() + CanvasUtilityDock::gap == layout.legend.getX());
        REQUIRE(layout.status.getCentreY() == layout.legend.getCentreY());
        REQUIRE(layout.status.getHeight() == 28.f);
        REQUIRE_FALSE(layout.status.intersects(layout.keyboard));
        REQUIRE(layout.footer.contains(layout.status));
        REQUIRE(layout.footer.contains(layout.legend));
    }
}

TEST_CASE("Centred MIDI dialog hard-stops only editor audition on pitch changes",
        "[cycle-v2][keyboard][sequence][editor-audition]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    Component host;
    host.setBounds(100, 100, 1400, 1000);
    PerformanceKeyboardPanel panel(state, sink);
    host.addAndMakeVisible(panel);
    panel.setBounds(0, 800, 1400, 180);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    panel.setSequence(phrase);
    panel.seekPlayback(0.25, 1000.0);
    panel.showSequenceEditor(phrase, [&](PresetMidiSequence next) { panel.setSequence(std::move(next)); });
    DialogWindow* dialog = nullptr;
    for (int index = 0; index < Desktop::getInstance().getNumComponents(); ++index) {
        auto* candidate = Desktop::getInstance().getComponent(index);
        if (candidate->getComponentID() == "PresetMidiEditor.Dialog") {
            dialog = dynamic_cast<DialogWindow*>(candidate);
        }
    }
    REQUIRE(dialog != nullptr);
    REQUIRE(dialog->getName() == "MIDI Pattern");
    auto* editor = dynamic_cast<PresetMidiEditor*>(dialog->getContentComponent());
    REQUIRE(editor != nullptr);
    REQUIRE(editor->getWidth() >= 900);
    REQUIRE(editor->getHeight() >= 640);
    const auto grid = editor->noteGrid();
    const float y = editor->pitchRow(60).getCentreY();
    const float nextY = editor->pitchRow(61).getCentreY();
    const float x = grid.getX() + grid.getWidth() * 0.25f;
    sink.messages.clear();
    sink.sources.clear();
    editor->mouseDown(keyboardMouseEvent(*editor, {x, y}, ModifierKeys::leftButtonModifier));
    editor->mouseDrag(keyboardMouseEvent(*editor, {x, nextY}, ModifierKeys::leftButtonModifier));
    editor->mouseUp(keyboardMouseEvent(*editor, {x, nextY}, ModifierKeys::leftButtonModifier));
    REQUIRE(panel.playbackPositionSeconds() == 0.25);
    REQUIRE(sink.messages.size() == 6);
    REQUIRE(sink.messages[0].isAllSoundOff());
    REQUIRE(sink.messages[1].isNoteOn());
    REQUIRE(sink.messages[1].getNoteNumber() == 60);
    REQUIRE(sink.messages[2].isNoteOff());
    REQUIRE(sink.messages[3].isAllSoundOff());
    REQUIRE(sink.messages[4].isNoteOn());
    REQUIRE(sink.messages[4].getNoteNumber() == 61);
    REQUIRE(sink.messages[5].isNoteOff());
    for (const auto source : sink.sources) {
        REQUIRE(source == MidiEventSource::EditorAudition);
    }
    editor->mouseDown(keyboardMouseEvent(*editor, {x, nextY}, ModifierKeys::leftButtonModifier));
    REQUIRE(sink.messages.back().isNoteOn());
    REQUIRE(editor->findParentComponentOfClass<DialogWindow>() == dialog);
    REQUIRE(dialog->isCurrentlyModal(false));
    Component::SafePointer<DialogWindow> closingDialog(dialog);
    REQUIRE(editor->keyPressed(KeyPress(KeyPress::escapeKey)));
    MessageManager::getInstance()->runDispatchLoopUntil(20);
    REQUIRE(sink.messages.back().isNoteOff());
    REQUIRE(sink.messages.back().getNoteNumber() == 61);
    if (closingDialog != nullptr) {
        REQUIRE_FALSE(closingDialog->isVisible());
        REQUIRE_FALSE(closingDialog->isCurrentlyModal(false));
        closingDialog.deleteAndZero();
    }
}

TEST_CASE("Pattern seeks are silent while stopped and resume overlapping notes at current automation",
        "[cycle-v2][keyboard][sequence][seek]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes = {{60, 90, 0.0, 1.0}, {64, 100, 0.5, 2.0}, {67, 80, 1.0, 1.0}};
    phrase.controls = {{1, 0, 0.0}, {1, 100, 2.0}, {73, 120, 0.0}, {73, 20, 2.0}};
    panel.setSequence(phrase);
    panel.seekPlayback(1.0, 2000.0);
    REQUIRE_FALSE(panel.isPlaying());
    REQUIRE(panel.playbackPositionSeconds() == 1.0);
    REQUIRE(sink.messages.empty());
    REQUIRE(panel.startPlayback(3000.0));
    int noteOns = 0;
    for (size_t index = 0; index < sink.messages.size(); ++index) {
        if (sink.sources[index] != MidiEventSource::PatternPlayback) {
            continue;
        }
        REQUIRE(sink.timestamps[index] >= 3.0);
        const auto& message = sink.messages[index];
        if (message.isNoteOn()) {
            REQUIRE(message.getNoteNumber() != 60);
            REQUIRE(sink.timestamps[index] == 3.0);
            ++noteOns;
        }
    }
    REQUIRE(noteOns == 2);
    const auto firstController = std::find_if(sink.messages.begin(), sink.messages.end(),
            [](const MidiMessage& message) { return message.isController() && message.getControllerValue() == 50; });
    REQUIRE(firstController != sink.messages.end());
    REQUIRE(panel.playbackProgress() == Catch::Approx(0.25));

    sink.messages.clear();
    sink.sources.clear();
    sink.timestamps.clear();
    panel.seekPlayback(2.25, 4000.0);
    REQUIRE(panel.isPlaying());
    REQUIRE(sink.releasedSources.back() == MidiEventSource::PatternPlayback);
    REQUIRE(panel.playbackPositionSeconds() == 2.25);
    noteOns = 0;
    for (size_t index = 0; index < sink.messages.size(); ++index) {
        if (sink.messages[index].isNoteOn()) {
            REQUIRE(sink.messages[index].getNoteNumber() == 64);
            REQUIRE(sink.timestamps[index] == 4.0);
            ++noteOns;
        }
        if (sink.messages[index].isNoteOff()) {
            REQUIRE(sink.timestamps[index] == 4.25);
        }
    }
    REQUIRE(noteOns == 1);
    panel.updatePlayback(4250.0);
    REQUIRE(panel.playbackPositionSeconds() == 2.5);
    REQUIRE(panel.heldNote() == -1);
    panel.stopPlayback(false);
    REQUIRE(panel.startPlayback(5000.0));
    REQUIRE(panel.playbackPositionSeconds() == 2.5);
    panel.seekPlayback(4.0, 6000.0);
    REQUIRE_FALSE(panel.isPlaying());
    REQUIRE(panel.playbackPositionSeconds() == 4.0);
    REQUIRE(panel.startPlayback(7000.0));
    REQUIRE(panel.playbackPositionSeconds() == 0.0);
}

TEST_CASE("Explicit seeking uses the full controller-only pattern timeline",
        "[cycle-v2][keyboard][sequence][seek]") {
    ScopedJuceInitialiser_GUI gui;
    MidiKeyboardState state;
    RecordingMidiSink sink;
    PerformanceKeyboardPanel panel(state, sink);
    panel.setPreviewNote(55);
    panel.setPlaybackDurationSeconds(2.f);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 10.0;
    phrase.controls = {{1, 20, 0.0}, {1, 100, 10.0}};
    panel.setSequence(phrase);
    panel.seekPlayback(7.0, 1000.0);
    REQUIRE(sink.messages.empty());
    REQUIRE(panel.playbackProgress() == Catch::Approx(0.7));
    REQUIRE(panel.startPlayback(2000.0));
    REQUIRE(panel.playbackPositionSeconds() == 7.0);
    REQUIRE(panel.heldNote() == 55);
    panel.updatePlayback(4000.0);
    REQUIRE(panel.isPlaying());
    REQUIRE(panel.playbackPositionSeconds() == 9.0);
    panel.updatePlayback(5000.0);
    REQUIRE_FALSE(panel.isPlaying());
    REQUIRE(panel.playbackPositionSeconds() == 10.0);
}
