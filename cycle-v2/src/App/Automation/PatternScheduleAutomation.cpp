#include <algorithm>

#include "App/Automation/PatternScheduleAutomation.h"

#include "App/Automation/CycleV2AutomationProtocol.h"
#include "UI/PatternPlaybackSchedule.h"

namespace CycleV2 {

namespace {

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

    juce::String error;
    const auto expanded = PatternPlaybackSchedule::expand(*sequence, error);
    if (!expanded.has_value()) {
        return failedResult("exportPatternSchedule", error);
    }
    juce::Array<juce::var> events;
    for (const auto& event : *expanded) {
        events.add(encodeEvent(event, channel));
    }
    auto data = std::make_unique<juce::DynamicObject>();
    data->setProperty("durationMs", sequence->durationSeconds * 1000.0);
    data->setProperty("scheduler", "PatternControlScheduler");
    data->setProperty("events", std::move(events));
    return okResult("exportPatternSchedule", juce::var(data.release()));
}

}
