#include <algorithm>

#include "App/PatternScheduleAutomation.h"

#include "App/CycleV2AutomationProtocol.h"
#include "UI/PatternPlaybackSchedule.h"

namespace CycleV2 {

namespace {

class PatternScheduleSink final : public MidiEventSink {
public:
    bool enqueueMidiMessage(const juce::MidiMessage& message, MidiEventSource source) override {
        return enqueueMidiMessageAt(message, source, 0.0);
    }

    bool enqueueMidiMessageAt(
            const juce::MidiMessage& message,
            MidiEventSource,
            double timestampSeconds) override {
        if (events.size() >= RealtimeMidiEventQueue::capacity) {
            return false;
        }
        events.push_back({ timestampSeconds, message });
        return true;
    }

    void releaseMidiSource(MidiEventSource) override {}

    std::vector<PatternPlaybackEvent> events;
};

juce::var encodeEvent(const PatternPlaybackEvent& event, int channel) {
    auto object = std::make_unique<juce::DynamicObject>();
    const auto& message = event.message;
    object->setProperty("timeMs", event.timeSeconds * 1000.0);
    object->setProperty("channel", channel);
    if (message.isController()) {
        object->setProperty("type", "controller");
        object->setProperty("controller", message.getControllerNumber());
        object->setProperty("value", message.getControllerValue());
    } else {
        object->setProperty("type", message.isNoteOn() ? "noteOn" : "noteOff");
        object->setProperty("note", message.getNoteNumber());
        if (message.isNoteOn()) {
            object->setProperty("velocity", message.getFloatVelocity());
        }
    }
    return { object.release() };
}

}

juce::var exportPatternSchedule(const juce::var& command) {
    using namespace AutomationProtocol;
    const auto* object = command.getDynamicObject();
    const auto sequence = PresetPresentationCodec::readSequenceJSON(
            object != nullptr ? object->getProperty("sequence") : juce::var());
    const int channel = intProperty(command, "channel", 1);
    if (!sequence.has_value() || channel < 1 || channel > 16) {
        return failedResult("exportPatternSchedule", "Invalid pattern sequence or MIDI channel");
    }

    auto prepared = PatternPlaybackSchedule::prepare(*sequence);
    PatternScheduleSink sink;
    for (auto& scheduler : prepared.controls) {
        if (!scheduler.scheduleUntil(sequence->durationSeconds, 0.0, sink)) {
            return failedResult("exportPatternSchedule", "Expanded pattern exceeds MIDI queue capacity");
        }
    }
    for (const auto& event : prepared.notes) {
        if (!sink.enqueueMidiMessageAt(event.message, MidiEventSource::PatternPlayback, event.timeSeconds)) {
            return failedResult("exportPatternSchedule", "Expanded pattern exceeds MIDI queue capacity");
        }
    }
    std::stable_sort(sink.events.begin(), sink.events.end(), PatternPlaybackSchedule::precedes);
    juce::Array<juce::var> events;
    for (const auto& event : sink.events) {
        events.add(encodeEvent(event, channel));
    }
    auto data = std::make_unique<juce::DynamicObject>();
    data->setProperty("durationMs", sequence->durationSeconds * 1000.0);
    data->setProperty("scheduler", "PatternControlScheduler");
    data->setProperty("events", std::move(events));
    return okResult("exportPatternSchedule", juce::var(data.release()));
}

}
