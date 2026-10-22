#include <catch2/catch_test_macros.hpp>

#include "App/Automation/PatternScheduleAutomation.h"
#include "Graph/PresetPresentation.h"
#include "UI/PatternPlaybackSchedule.h"

using namespace CycleV2;

TEST_CASE("Pattern schedule export retains interpolated controllers and note boundary order",
        "[cycle-v2][pattern-schedule][automation]") {
    PresetMidiSequence sequence;
    sequence.durationSeconds = 1.0;
    sequence.notes = { { 60, 92, 0.0, 0.5 }, { 60, 80, 0.5, 0.25 } };
    sequence.controls = { { 74, 0, 0.0 }, { 74, 100, 0.5 } };
    auto command = std::make_unique<juce::DynamicObject>();
    command->setProperty("sequence", PresetPresentationCodec::writeSequenceJSON(sequence));
    command->setProperty("channel", 3);
    const auto result = exportPatternSchedule(juce::var(command.release()));
    REQUIRE((bool) result["ok"]);
    const auto data = result["data"];
    const auto eventsValue = data["events"];
    const auto* events = eventsValue.getArray();
    REQUIRE(events != nullptr);
    REQUIRE(events->size() == 105);
    REQUIRE((*events)[0]["type"].toString() == "controller");
    REQUIRE((int) (*events)[0]["value"] == 0);
    REQUIRE((*events)[1]["type"].toString() == "noteOn");

    juce::StringArray atBoundary;
    bool interpolated = false;
    for (const auto& event : *events) {
        REQUIRE((int) event["channel"] == 3);
        if ((double) event["timeMs"] == 500.0) {
            atBoundary.add(event["type"].toString());
        }
        if (event["type"].toString() == "controller"
                && (int) event["value"] == 50) {
            REQUIRE((double) event["timeMs"] == 250.0);
            interpolated = true;
        }
    }
    REQUIRE(interpolated);
    const juce::StringArray expectedOrder { "noteOff", "controller", "noteOn" };
    REQUIRE(atBoundary == expectedOrder);
}

TEST_CASE("Pattern schedule export rejects malformed and oversized performances atomically",
        "[cycle-v2][pattern-schedule][automation]") {
    const auto invalid = exportPatternSchedule(juce::JSON::parse(R"({"sequence":{"durationSeconds":0}})"));
    REQUIRE_FALSE((bool) invalid["ok"]);

    PresetMidiSequence sequence;
    sequence.durationSeconds = 1.0;
    sequence.notes = { { 60, 100, 0.0, 0.5 } };
    for (int controller = 0; controller < 128; ++controller) {
        sequence.controls.push_back({ controller, 0, 0.0 });
        sequence.controls.push_back({ controller, 127, 1.0 });
    }
    auto command = std::make_unique<juce::DynamicObject>();
    command->setProperty("sequence", PresetPresentationCodec::writeSequenceJSON(sequence));
    const auto result = exportPatternSchedule(juce::var(command.release()));
    REQUIRE_FALSE((bool) result["ok"]);
    REQUIRE(result["message"].toString().contains("capacity"));
    REQUIRE(result["data"].isVoid());
}
