#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Nodes/Control/ModulationSource.h"
#include "../src/Runtime/AudioProcessContextUtils.h"

using namespace CycleV2;

namespace {

std::shared_ptr<const ModulationSourceConfiguration> configuration(
        const String& source,
        int controller = 1,
        float constant = 0.5f) {
    return ModulationSource::buildConfiguration({
            { "source", "Source", source },
            { "controller", "Controller", String(controller) },
            { "constant", "Constant", String(constant) }
    });
}

AudioProcessContext processContext(
        size_t frameCount,
        const AudioVoiceContext& voice) {
    AudioProcessContext context;
    context.frameCount = frameCount;
    context.voiceView = &voice;
    context.outputPorts.push_back({ "value", PortDomain::ControlSignal, ChannelLayout::Mono });
    return context;
}

std::vector<float> renderAudio(
        const ModulationSourceConfiguration& configurationToUse,
        const AudioVoiceContext& voice,
        size_t frameCount) {
    auto processor = createModulationSourceAudioProcessor();
    const auto ownership = std::make_shared<ModulationSourceConfiguration>(configurationToUse);
    processor->adoptConfiguration({ 1, "modulation", ownership });
    auto context = processContext(frameCount, voice);
    processor->process(context);
    REQUIRE(context.outputs.size() == 1);
    return context.outputs.front().block.samples;
}

}

TEST_CASE("Modulation source evaluates Cycle v1 control semantics",
        "[cycle-v2][modulation][control]") {
    PreviewControlContext context;
    context.voiceTime = 0.25f;
    context.velocity = 0.2f;
    context.noteNumber = 64;
    context.lowestNote = 0;
    context.highestNote = 127;
    context.channelPressure = 0.75f;
    context.controllers[1] = 0.3f;
    context.controllers[74] = 0.6f;

    REQUIRE(ModulationSource::evaluate(*configuration("voiceTime"), context) == 0.25f);
    REQUIRE(ModulationSource::evaluate(*configuration("velocity"), context) == 0.2f);
    REQUIRE(ModulationSource::evaluate(*configuration("inverseVelocity"), context)
            == Catch::Approx(0.8f));
    REQUIRE(ModulationSource::evaluate(*configuration("keyScale"), context)
            == Catch::Approx(64.f / 127.f));
    REQUIRE(ModulationSource::evaluate(*configuration("modWheel"), context) == 0.3f);
    REQUIRE(ModulationSource::evaluate(*configuration("midiCC", 74), context) == 0.6f);
    REQUIRE(ModulationSource::evaluate(*configuration("channelPressure"), context) == 0.75f);
    REQUIRE(ModulationSource::evaluate(*configuration("constant", 1, 0.4f), context) == 0.4f);

    REQUIRE(ModulationSource::normalizeKey(10, 10, 10) == 0.f);
    REQUIRE(ModulationSource::normalizeKey(-10, 0, 127) == 0.f);
    REQUIRE(ModulationSource::normalizeKey(140, 0, 127) == 1.f);
}

TEST_CASE("Mod wheel and MIDI CC 1 share controller state",
        "[cycle-v2][modulation][control]") {
    PreviewControlContext context;
    context.controllers[1] = 0.73f;

    REQUIRE(ModulationSource::evaluate(*configuration("modWheel"), context)
            == ModulationSource::evaluate(*configuration("midiCC", 1), context));
}

TEST_CASE("Timed controller changes produce sample-accurate held spans",
        "[cycle-v2][modulation][audio]") {
    AudioVoiceContext voice;
    voice.controls.controllers[74] = 0.1f;
    voice.controlEvents = {
            { ControlEventKind::Controller, 2, 74, 0.5f },
            { ControlEventKind::Controller, 5, 74, 0.9f }
    };

    const auto output = renderAudio(*configuration("midiCC", 74), voice, 8);
    REQUIRE(output == std::vector<float> {
            0.1f, 0.1f, 0.5f, 0.5f, 0.5f, 0.9f, 0.9f, 0.9f
    });
}

TEST_CASE("Unrelated controller events do not change a selected source",
        "[cycle-v2][modulation][audio]") {
    AudioVoiceContext voice;
    voice.controls.controllers[1] = 0.4f;
    voice.controlEvents = {
            { ControlEventKind::Controller, 3, 74, 1.f }
    };

    const auto output = renderAudio(*configuration("modWheel"), voice, 6);
    REQUIRE(output == std::vector<float>(6, 0.4f));
}

TEST_CASE("Channel pressure events are independent of MIDI controllers",
        "[cycle-v2][modulation][audio]") {
    AudioVoiceContext voice;
    voice.controls.channelPressure = 0.2f;
    voice.controls.controllers[1] = 0.8f;
    voice.controlEvents = {
            { ControlEventKind::ChannelPressure, 3, 0, 0.7f }
    };

    REQUIRE(renderAudio(*configuration("channelPressure"), voice, 5)
            == std::vector<float> { 0.2f, 0.2f, 0.2f, 0.7f, 0.7f });
    REQUIRE(renderAudio(*configuration("modWheel"), voice, 5)
            == std::vector<float>(5, 0.8f));
}

TEST_CASE("Modulation preview is deterministic from explicit audition controls",
        "[cycle-v2][modulation][preview]") {
    PreviewControlContext controls;
    controls.noteNumber = 96;
    controls.lowestNote = 0;
    controls.highestNote = 127;
    auto processor = createModulationSourcePreviewProcessor();
    PreviewProcessContext context;
    context.pointCount = 7;
    context.parameters = {
            { "source", "Source", "keyScale" }
    };
    context.controlContext = &controls;

    processor->render(context);

    REQUIRE(context.domain == PortDomain::ControlSignal);
    REQUIRE(context.primary == std::vector<float>(7, 96.f / 127.f));
    controls.noteNumber = 12;
    REQUIRE(context.primary == std::vector<float>(7, 96.f / 127.f));
}
