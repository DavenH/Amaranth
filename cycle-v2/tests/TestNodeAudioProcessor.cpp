#include <catch2/catch_test_macros.hpp>

#include "../src/Runtime/NodeAudioProcessor.h"

using namespace CycleV2;

namespace {

AudioProcessBlock block(std::initializer_list<float> samples) {
    return { std::vector<float>(samples) };
}

}

TEST_CASE("Node audio processor factory creates executable modules", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    auto processor = factory.create(AudioModuleRole::Multiply);
    REQUIRE(processor != nullptr);
    REQUIRE(processor->role() == AudioModuleRole::Multiply);

    REQUIRE(factory.create(AudioModuleRole::None) == nullptr);
}

TEST_CASE("Source audio processors produce deterministic buffers", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::WaveSource);
    REQUIRE(processor != nullptr);

    AudioProcessContext context;
    context.frameCount = 5;
    processor->process(context);

    REQUIRE(context.output.samples == std::vector<float> { 0.f, 0.25f, 0.5f, 0.75f, 1.f });
}

TEST_CASE("Audio processors read node parameters from process context", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext sourceContext;
    sourceContext.frameCount = 3;
    sourceContext.parameters = { { "level", "Level", "0.5" } };
    factory.create(AudioModuleRole::WaveSource)->process(sourceContext);

    REQUIRE(sourceContext.output.samples == std::vector<float> { 0.f, 0.25f, 0.5f });

    AudioProcessContext envelopeContext;
    envelopeContext.frameCount = 2;
    envelopeContext.parameters = { { "level", "Level", "0.25" } };
    factory.create(AudioModuleRole::Envelope)->process(envelopeContext);

    REQUIRE(envelopeContext.output.samples == std::vector<float> { 0.25f, 0.25f });
}

TEST_CASE("Utility audio processors add and multiply inputs", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext addContext;
    addContext.frameCount = 4;
    addContext.inputs = { block({ 1.f, 2.f, 3.f, 4.f }), block({ 0.5f, 1.f, 1.5f, 2.f }) };
    factory.create(AudioModuleRole::Add)->process(addContext);

    REQUIRE(addContext.output.samples == std::vector<float> { 1.5f, 3.f, 4.5f, 6.f });

    AudioProcessContext multiplyContext;
    multiplyContext.frameCount = 4;
    multiplyContext.inputs = { block({ 1.f, 2.f, 3.f, 4.f }), block({ 0.5f, 1.f, 1.5f, 2.f }) };
    factory.create(AudioModuleRole::Multiply)->process(multiplyContext);

    REQUIRE(multiplyContext.output.samples == std::vector<float> { 0.5f, 2.f, 4.5f, 8.f });
}

TEST_CASE("Adapter placeholder audio processors pass through first input", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::Waveshaper);
    REQUIRE(processor != nullptr);

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = { block({ -0.25f, 0.f, 0.5f }) };
    processor->process(context);

    REQUIRE(context.output.samples == std::vector<float> { -0.25f, 0.f, 0.5f });
}

TEST_CASE("FFT placeholder processor publishes separate magnitude and phase ports", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::Fft);
    REQUIRE(processor != nullptr);

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = { block({ 0.f, 0.5f, 1.f }) };
    context.outputPorts = {
            { "mag", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo },
            { "phase", PortDomain::SpectralPhaseSignal, ChannelLayout::LinkedStereo }
    };
    processor->process(context);

    REQUIRE(context.outputs.size() == 2);
    REQUIRE(context.outputs[0].domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(context.outputs[0].samples == std::vector<float> { 0.f, 0.5f, 1.f });
    REQUIRE(context.outputs[1].domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(context.outputs[1].samples == std::vector<float> { 0.f, 0.f, 0.f });
}

TEST_CASE("Mesh source processor generates a deterministic operand when unconnected", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 3;
    context.outputPorts = {
            { "out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo }
    };
    factory.create(AudioModuleRole::MeshSource)->process(context);

    REQUIRE(context.output.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(context.output.samples == std::vector<float> { -1.f, 0.f, 1.f });
}

TEST_CASE("Mesh source processor passes through ordinary signal inputs", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = {
            { std::vector<float> { 0.f, 0.f, 0.f }, PortDomain::DomainContext, ChannelLayout::Mono },
            { std::vector<float> { 0.25f, 0.5f, 0.75f }, PortDomain::TimeSignal, ChannelLayout::LinkedStereo }
    };
    context.outputPorts = {
            { "out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo }
    };
    factory.create(AudioModuleRole::MeshSource)->process(context);

    REQUIRE(context.output.samples == std::vector<float> { 0.25f, 0.5f, 0.75f });
}
