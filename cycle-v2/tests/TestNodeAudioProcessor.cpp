#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Runtime/NodeAudioProcessor.h"

#include <algorithm>

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

TEST_CASE("FFT cycle processor publishes separate magnitude and phase ports", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::Fft);
    REQUIRE(processor != nullptr);

    AudioProcessContext context;
    context.frameCount = 4;
    context.inputs = { block({ -0.5f, -0.25f, 0.25f, 0.5f }) };
    context.outputPorts = {
            { "mag", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo },
            { "phase", PortDomain::SpectralPhaseSignal, ChannelLayout::LinkedStereo }
    };
    processor->process(context);

    REQUIRE(context.outputs.size() == 2);
    REQUIRE(context.outputs[0].domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(context.outputs[0].samples.size() == 4);
    REQUIRE(context.outputs[0].samples[0] > 0.f);
    REQUIRE(context.outputs[1].domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(context.outputs[1].samples.size() == 4);
}

TEST_CASE("FFT and IFFT cycle processors round trip zero-mean cycle buffers", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext fftContext;
    fftContext.frameCount = 4;
    fftContext.inputs = { block({ -0.5f, -0.25f, 0.25f, 0.5f }) };
    fftContext.outputPorts = {
            { "mag", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo },
            { "phase", PortDomain::SpectralPhaseSignal, ChannelLayout::LinkedStereo }
    };
    factory.create(AudioModuleRole::Fft)->process(fftContext);

    AudioProcessContext ifftContext;
    ifftContext.frameCount = 4;
    ifftContext.inputs = { fftContext.outputs[0], fftContext.outputs[1] };
    ifftContext.outputPorts = {
            { "time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo }
    };
    factory.create(AudioModuleRole::Ifft)->process(ifftContext);

    REQUIRE(ifftContext.output.samples[0] == Catch::Approx(-0.5f).margin(1.0e-5f));
    REQUIRE(ifftContext.output.samples[1] == Catch::Approx(-0.25f).margin(1.0e-5f));
    REQUIRE(ifftContext.output.samples[2] == Catch::Approx(0.25f).margin(1.0e-5f));
    REQUIRE(ifftContext.output.samples[3] == Catch::Approx(0.5f).margin(1.0e-5f));
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
    REQUIRE(context.output.samples.size() == 3);
    REQUIRE(*std::min_element(context.output.samples.begin(), context.output.samples.end())
            < *std::max_element(context.output.samples.begin(), context.output.samples.end()));
}

TEST_CASE("Mesh source processor uses non-cyclic rendering for spectral outputs", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto timeProcessor = factory.create(AudioModuleRole::MeshSource);
    auto spectralProcessor = factory.create(AudioModuleRole::MeshSource);

    AudioProcessContext timeContext;
    timeContext.frameCount = 12;
    timeContext.outputPorts = {
            { "out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo }
    };

    AudioProcessContext spectralContext;
    spectralContext.frameCount = 12;
    spectralContext.outputPorts = {
            { "out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo }
    };

    timeProcessor->process(timeContext);
    spectralProcessor->process(spectralContext);

    REQUIRE(timeContext.output.domain == PortDomain::TimeSignal);
    REQUIRE(spectralContext.output.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(timeContext.output.samples != spectralContext.output.samples);
}

TEST_CASE("Mesh source processor defensively ignores unexpected signal inputs", "[cycle-v2][runtime]") {
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

    REQUIRE(context.output.samples.size() == 3);
    REQUIRE(*std::min_element(context.output.samples.begin(), context.output.samples.end())
            < *std::max_element(context.output.samples.begin(), context.output.samples.end()));
}
