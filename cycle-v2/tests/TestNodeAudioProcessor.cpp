#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Runtime/AudioProcessContextUtils.h"
#include "../src/Runtime/NodeAudioProcessor.h"
#include "../src/Runtime/SmoothedMorphPosition.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"
#include "../src/Nodes/Envelope/EnvelopeMeshState.h"
#include "../src/Nodes/Envelope/EnvelopePreparationExchange.h"
#include "../src/Nodes/Envelope/EnvelopeSignalProcessor.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numeric>
#include <thread>

using namespace CycleV2;

TEST_CASE("Morph controls smooth monotonically and independently of block partitioning",
        "[cycle-v2][runtime][morph][smoothing]") {
    SmoothedMorphPosition whole;
    SmoothedMorphPosition partitioned;
    const MorphPosition initial(0.f, 0.f, 0.75f);
    const MorphPosition target(1.f, 1.f, 0.25f);
    whole.reset(initial);
    partitioned.reset(initial);
    whole.setTargets(target);
    partitioned.setTargets(target);

    REQUIRE(whole.advance(64, 44100.0) == 64);
    REQUIRE(partitioned.advance(32, 44100.0) == 32);
    const float halfway = partitioned.current().red.getCurrentValue();
    REQUIRE(partitioned.advance(32, 44100.0) == 32);

    REQUIRE(halfway > 0.f);
    REQUIRE(halfway < whole.current().red.getCurrentValue());
    REQUIRE(whole.current().red.getCurrentValue() < 1.f);
    REQUIRE(partitioned.current().red.getCurrentValue()
            == Catch::Approx(whole.current().red.getCurrentValue()));
    REQUIRE(partitioned.current().blue.getCurrentValue()
            == Catch::Approx(whole.current().blue.getCurrentValue()));
}

TEST_CASE("Envelope preparation request exchange publishes coherent newest values",
        "[cycle-v2][runtime][envelope][exchange]") {
    LatestEnvelopePreparationRequest requests;
    requests.publish(0.2f, 0.8f, 11);
    requests.publish(0.7f, 0.3f, 12);

    EnvelopePreparationRequest request;
    REQUIRE(requests.latest(request));
    REQUIRE(request.red == 0.7f);
    REQUIRE(request.blue == 0.3f);
    REQUIRE(request.noteSerial == 12);
    REQUIRE(requests.isCurrent(request.generation));
    requests.markPrepared(request.generation);
    REQUIRE_FALSE(requests.latest(request));
}

TEST_CASE("Envelope preparation request exchange never mixes concurrent fields",
        "[cycle-v2][runtime][envelope][exchange]") {
    LatestEnvelopePreparationRequest requests;
    std::atomic<bool> writerFinished {};
    std::atomic<bool> coherent { true };
    std::thread writer([&] {
        for (uint64_t i = 1; i <= 10000; ++i) {
            const float red = (float) i / 10000.f;
            requests.publish(red, 1.f - red, i);
        }
        writerFinished = true;
    });

    uint64_t observedGeneration = 0;
    while (!writerFinished.load() || requests.publicationCount() < 10000) {
        EnvelopePreparationRequest request;
        if (!requests.latest(request)) {
            continue;
        }
        observedGeneration = request.generation;
        requests.markPrepared(observedGeneration);
        const float expectedRed = (float) request.noteSerial / 10000.f;
        if (request.red != expectedRed || request.blue != 1.f - expectedRed) {
            coherent = false;
        }
    }
    writer.join();

    REQUIRE(coherent.load());
    REQUIRE(requests.publicationCount() == 10000);
}

TEST_CASE("Prepared Envelope exchange rejects stale notes and bounds slot ownership",
        "[cycle-v2][runtime][envelope][exchange]") {
    const std::vector<NodeParameter> parameters;
    const auto configuration = EnvelopeSignalProcessor::buildConfiguration(parameters);
    REQUIRE(configuration != nullptr);
    PreparedEnvelopeExchange exchange;

    REQUIRE(exchange.publish(configuration, 2, 1));
    const auto stale = exchange.adoptNewest(2, false);
    REQUIRE(stale.stale);
    REQUIRE_FALSE(stale.adopted);

    REQUIRE(exchange.publish(configuration, 4, 2));
    const auto adopted = exchange.adoptNewest(2, false);
    REQUIRE(adopted.adopted);
    REQUIRE(adopted.configuration == configuration.get());
    REQUIRE(exchange.active(nullptr) == configuration.get());

    REQUIRE(exchange.publish(configuration, 6, 2));
    REQUIRE(exchange.adoptNewest(2, true).adopted);
    REQUIRE(exchange.preparationCount() == 3);
    REQUIRE(exchange.adoptionCount() == 2);
    REQUIRE(exchange.staleResultCount() == 1);
}

namespace {

std::vector<NodeParameter> curveParameters(std::vector<FlatCurveVertex> vertices) {
    (void) vertices;
    return {};
}

NodeModelStatePtr curveModel(std::vector<FlatCurveVertex> vertices) {
    FlatCurveModel model;
    REQUIRE(model.replaceVertices(std::move(vertices)));
    return std::make_shared<const CurveNodeModelState>(
            "flatCurve",
            FlatCurveModel::currentVersion,
            model.revision(),
            model.writeJSON());
}

std::vector<NodeParameter> envelopeParameters(const String& payload = EnvelopeMeshState::defaultSnapshot()) {
    (void) payload;
    return {};
}

SignalPayload payload(std::initializer_list<float> samples) {
    SignalPayload result;
    result.block.samples = std::vector<float>(samples);
    result.domain = PortDomain::TimeSignal;
    result.channelLayout = ChannelLayout::LinkedStereo;
    return result;
}

SignalPayload payload(const std::vector<float>& samples) {
    SignalPayload result;
    result.block.samples = samples;
    result.domain = PortDomain::TimeSignal;
    result.channelLayout = ChannelLayout::LinkedStereo;
    return result;
}

SignalPayload gridPayload(std::initializer_list<float> samples, size_t columns, size_t rows) {
    SignalPayload result = payload(samples);
    result.traversalGrid.values = std::vector<float>(samples);
    result.traversalGrid.columns = columns;
    result.traversalGrid.rows = rows;
    result.traversalGrid.metadata = makeTraversalGridMetadata(
            result.domain,
            columns,
            rows,
            TraversalGridAxis::Repeated,
            TraversalGridAxis::Time);
    return result;
}

SignalPayload gridPayload(const std::vector<float>& samples, size_t columns, size_t rows) {
    REQUIRE(samples.size() >= rows);
    SignalPayload result;
    result.block.samples.assign(samples.begin(), samples.begin() + (std::ptrdiff_t) rows);
    result.domain = PortDomain::TimeSignal;
    result.channelLayout = ChannelLayout::LinkedStereo;
    result.traversalGrid.values = samples;
    result.traversalGrid.columns = columns;
    result.traversalGrid.rows = rows;
    result.traversalGrid.metadata = makeTraversalGridMetadata(
            result.domain,
            columns,
            rows,
            TraversalGridAxis::Repeated,
            TraversalGridAxis::Time);
    return result;
}

const SignalPayload& output(const AudioProcessContext& context, size_t index = 0) {
    REQUIRE(context.outputs.size() > index);
    return context.outputs[index];
}

void prepareProcessor(
        NodeAudioProcessor& processor,
        AudioModuleRole role,
        const AudioProcessContext& context,
        size_t maximumFrameCount = 0,
        uint64_t revision = 1,
        NodeModelStatePtr model = {}) {
    AudioExecutionSpec spec;
    spec.maximumFrameCount = maximumFrameCount == 0 ? context.frameCount : maximumFrameCount;
    spec.sampleRate = context.timing.sampleRate;
    spec.bpm = context.timing.bpm;
    spec.beatsPerMeasure = context.timing.beatsPerMeasure;
    const auto configuration = NodeDspConfigurationFactory().create(
            role, context.parameters, std::move(model), spec);
    REQUIRE(configuration != nullptr);
    processor.adoptConfiguration({ revision, "test-configuration", configuration });
    processor.prepareExecution(spec);
}

}

TEST_CASE("Node audio processor factory creates executable modules", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    const AudioModuleRole roles[] {
            AudioModuleRole::VoiceContext,
            AudioModuleRole::WaveSource,
            AudioModuleRole::ImageSource,
            AudioModuleRole::MeshSource,
            AudioModuleRole::Fft,
            AudioModuleRole::Ifft,
            AudioModuleRole::Add,
            AudioModuleRole::Multiply,
            AudioModuleRole::Envelope,
            AudioModuleRole::GuideCurve,
            AudioModuleRole::ImpulseResponse,
            AudioModuleRole::Waveshaper,
            AudioModuleRole::Reverb,
            AudioModuleRole::Delay,
            AudioModuleRole::Equalizer,
            AudioModuleRole::StereoSplit,
            AudioModuleRole::StereoJoin,
            AudioModuleRole::Output,
            AudioModuleRole::GenericProcessor
    };

    for (const AudioModuleRole role : roles) {
        CAPTURE(role);
        auto processor = factory.create(role);
        REQUIRE(processor != nullptr);
        REQUIRE(processor->role() == role);
    }

    REQUIRE(factory.create(AudioModuleRole::None) == nullptr);
}

TEST_CASE("Source audio processors produce deterministic buffers", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::WaveSource);
    REQUIRE(processor != nullptr);

    AudioProcessContext context;
    context.frameCount = 5;
    context.outputPorts = {
            { "out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo }
    };
    processor->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { 0.f, 0.25f, 0.5f, 0.75f, 1.f });
    REQUIRE(output(context).traversalGrid.isValid());
    REQUIRE(output(context).traversalGrid.rows == 5);
    REQUIRE(output(context).traversalGrid.values.size() == output(context).traversalGrid.columns * 5);
    REQUIRE(output(context).traversalGrid.metadata.arity == TraversalGridArity::Matrix);
    REQUIRE(output(context).traversalGrid.metadata.valueDomain == PortDomain::TimeSignal);
    REQUIRE(output(context).traversalGrid.metadata.columnAxis == TraversalGridAxis::Phase);
    REQUIRE(output(context).traversalGrid.metadata.rowAxis == TraversalGridAxis::Time);
    REQUIRE(output(context).traversalGrid.values[0] == Catch::Approx(0.f));
    REQUIRE(output(context).traversalGrid.values[5] > output(context).traversalGrid.values[0]);
    REQUIRE(output(context).traversalGrid.values[5] != Catch::Approx(output(context).traversalGrid.values[0]));
}

TEST_CASE("Image source publishes a two-dimensional traversal grid", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::ImageSource);
    REQUIRE(processor != nullptr);

    AudioProcessContext context;
    context.frameCount = 4;
    processor->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { 0.f, 1.f / 3.f, 2.f / 3.f, 1.f });
    REQUIRE(output(context).traversalGrid.isValid());
    REQUIRE(output(context).traversalGrid.rows == 4);
    REQUIRE(output(context).traversalGrid.columns == 8);
    REQUIRE(output(context).traversalGrid.metadata.arity == TraversalGridArity::Matrix);
    REQUIRE(output(context).traversalGrid.metadata.columnAxis == TraversalGridAxis::ImageX);
    REQUIRE(output(context).traversalGrid.metadata.rowAxis == TraversalGridAxis::ImageY);
    REQUIRE(output(context).traversalGrid.values[0] == Catch::Approx(0.f));
    REQUIRE(output(context).traversalGrid.values[3] == Catch::Approx(0.35f));
    REQUIRE(output(context).traversalGrid.values[4] == Catch::Approx(0.65f / 7.f));
}

TEST_CASE("Audio processors read node parameters from process context", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext sourceContext;
    sourceContext.frameCount = 3;
    sourceContext.parameters = { { "level", "Level", "0.5" } };
    factory.create(AudioModuleRole::WaveSource)->process(sourceContext);

    REQUIRE(output(sourceContext).block.samples == std::vector<float> { 0.f, 0.25f, 0.5f });

    AudioProcessContext envelopeContext;
    envelopeContext.frameCount = 4;
    envelopeContext.timing.sampleRate = 8.0;
    envelopeContext.parameters = envelopeParameters();
    envelopeContext.parameters.push_back({ "level", "Level", "0.25" });
    envelopeContext.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    auto envelopeProcessor = factory.create(AudioModuleRole::Envelope);
    prepareProcessor(*envelopeProcessor, AudioModuleRole::Envelope, envelopeContext);
    envelopeProcessor->process(envelopeContext);

    REQUIRE(output(envelopeContext).block.samples.front() < 0.001f);
    REQUIRE(output(envelopeContext).block.samples.back() > output(envelopeContext).block.samples.front());
    REQUIRE(*std::max_element(
            output(envelopeContext).block.samples.begin(),
            output(envelopeContext).block.samples.end()) <= 0.25f);
}

TEST_CASE("Envelope processor applies sample-offset lifecycle events", "[cycle-v2][runtime][envelope]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::Envelope);
    const std::vector<NodeParameter> parameters = envelopeParameters();

    AudioProcessContext noteOn;
    noteOn.frameCount = 8;
    noteOn.timing.sampleRate = 16.0;
    noteOn.parameters = parameters;
    noteOn.voice.events.push_back({ NoteLifecycleType::NoteOn, 3, 0 });
    prepareProcessor(*processor, AudioModuleRole::Envelope, noteOn);
    processor->process(noteOn);

    REQUIRE(output(noteOn).block.samples[0] == Catch::Approx(0.f));
    REQUIRE(output(noteOn).block.samples[2] == Catch::Approx(0.f));
    REQUIRE(output(noteOn).block.samples[4] > 0.f);

    AudioProcessContext noteOff;
    noteOff.frameCount = 8;
    noteOff.timing.sampleRate = 16.0;
    noteOff.parameters = parameters;
    noteOff.voice.events.push_back({ NoteLifecycleType::NoteOff, 0, 0 });
    processor->process(noteOff);

    REQUIRE(output(noteOff).block.samples.front() > 0.f);

    AudioProcessContext reset;
    reset.frameCount = 4;
    reset.timing.sampleRate = 16.0;
    reset.parameters = parameters;
    reset.voice.events.push_back({ NoteLifecycleType::Reset, 0, 0 });
    processor->process(reset);

    REQUIRE(output(reset).block.samples == std::vector<float> { 0.f, 0.f, 0.f, 0.f });

    AudioProcessContext retrigger;
    retrigger.frameCount = 4;
    retrigger.timing.sampleRate = 16.0;
    retrigger.parameters = parameters;
    retrigger.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    processor->process(retrigger);

    REQUIRE(output(retrigger).block.samples.front() < 0.001f);
    REQUIRE(output(retrigger).block.samples.back() > 0.f);
}

TEST_CASE("Envelope processor maps morph and logarithmic parameters", "[cycle-v2][runtime][envelope]") {
    NodeAudioProcessorFactory factory;
    const String snapshot = EnvelopeMeshState::defaultSnapshot();
    auto render = [&](float red, bool logarithmic) {
        AudioProcessContext context;
        context.frameCount = 16;
        context.timing.sampleRate = 16.0;
        context.parameters = envelopeParameters(snapshot);
        context.parameters.insert(context.parameters.end(), {
                { "red", "Red", String(red) },
                { "blue", "Blue", "0" },
                { "logarithmic", "Logarithmic", logarithmic ? "1" : "0" }
        });
        context.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
        auto processor = factory.create(AudioModuleRole::Envelope);
        prepareProcessor(*processor, AudioModuleRole::Envelope, context);
        processor->process(context);
        return output(context).block.samples;
    };

    const auto linear = render(0.f, false);
    const auto morphed = render(1.f, false);
    const auto logarithmic = render(0.f, true);

    REQUIRE(morphed != linear);
    REQUIRE(logarithmic != linear);
}

TEST_CASE("Envelope traversal samples phase across grid columns", "[cycle-v2][runtime][envelope][grid]") {
    NodeAudioProcessorFactory factory;
    const auto parameters = envelopeParameters();
    AudioProcessContext envelopeContext;
    envelopeContext.frameCount = 4;
    envelopeContext.timing.sampleRate = 16.0;
    envelopeContext.parameters = parameters;
    envelopeContext.outputPorts = {
            { "env", PortDomain::EnvelopeSignal, ChannelLayout::Mono }
    };
    envelopeContext.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    auto envelopeProcessor = factory.create(AudioModuleRole::Envelope);
    prepareProcessor(*envelopeProcessor, AudioModuleRole::Envelope, envelopeContext);
    envelopeProcessor->process(envelopeContext);

    const SignalPayload& envelope = output(envelopeContext);
    REQUIRE(envelope.traversalGrid.columns == 8);
    REQUIRE(envelope.traversalGrid.rows == 4);
    REQUIRE(envelope.traversalGrid.metadata.columnAxis == TraversalGridAxis::Time);
    REQUIRE(envelope.traversalGrid.metadata.rowAxis == TraversalGridAxis::Repeated);

    const auto configuration = EnvelopeSignalProcessor::buildConfiguration(parameters);
    REQUIRE(configuration != nullptr);
    auto sampler = configuration->rasterizer->sampler();
    REQUIRE(sampler.isSampleable());

    for (size_t column = 0; column < envelope.traversalGrid.columns; ++column) {
        const float columnValue = envelope.traversalGrid.values[column * envelope.traversalGrid.rows];
        const float expected = sampler.sampleAt(
                (double) column / (double) envelope.traversalGrid.columns);
        REQUIRE(columnValue == Catch::Approx(expected));
        for (size_t row = 1; row < envelope.traversalGrid.rows; ++row) {
            REQUIRE(envelope.traversalGrid.values[column * envelope.traversalGrid.rows + row]
                    == Catch::Approx(columnValue));
        }
    }

    AudioProcessContext tallerContext;
    tallerContext.frameCount = 6;
    tallerContext.timing.sampleRate = 16.0;
    tallerContext.parameters = parameters;
    tallerContext.outputPorts = envelopeContext.outputPorts;
    tallerContext.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    auto tallerProcessor = factory.create(AudioModuleRole::Envelope);
    prepareProcessor(*tallerProcessor, AudioModuleRole::Envelope, tallerContext);
    tallerProcessor->process(tallerContext);

    const SignalTraversalGrid& taller = output(tallerContext).traversalGrid;
    REQUIRE(taller.columns == envelope.traversalGrid.columns);
    REQUIRE(taller.rows == 6);
    for (size_t column = 0; column < taller.columns; ++column) {
        REQUIRE(taller.values[column * taller.rows]
                == Catch::Approx(envelope.traversalGrid.values[column * envelope.traversalGrid.rows]));
    }

    AudioProcessContext widerContext;
    widerContext.frameCount = 12;
    widerContext.timing.sampleRate = 16.0;
    widerContext.parameters = parameters;
    widerContext.outputPorts = envelopeContext.outputPorts;
    widerContext.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    auto widerProcessor = factory.create(AudioModuleRole::Envelope);
    prepareProcessor(*widerProcessor, AudioModuleRole::Envelope, widerContext);
    widerProcessor->process(widerContext);

    const SignalTraversalGrid& wider = output(widerContext).traversalGrid;
    REQUIRE(wider.columns == 12);
    for (size_t column = 0; column < wider.columns; ++column) {
        REQUIRE(wider.values[column * wider.rows] == Catch::Approx(sampler.sampleAt(
                (double) column / (double) wider.columns)));
    }

    REQUIRE(envelope.traversalGrid.values[0]
            != Catch::Approx(envelope.traversalGrid.values[4 * envelope.traversalGrid.rows]));

    SignalPayload input = gridPayload(std::vector<float>(16, 1.f), 4, 4);
    input.traversalGrid.metadata.columnAxis = TraversalGridAxis::Phase;
    AudioProcessContext multiplyContext;
    multiplyContext.frameCount = 4;
    multiplyContext.inputs = { std::move(input), envelope };
    factory.create(AudioModuleRole::Multiply)->process(multiplyContext);

    const SignalTraversalGrid& multiplied = output(multiplyContext).traversalGrid;
    REQUIRE(multiplied.isValid());
    REQUIRE(multiplied.values[0] == Catch::Approx(envelope.traversalGrid.values[0]));
    REQUIRE(multiplied.columns == 4);
    REQUIRE(multiplied.values[2 * multiplied.rows]
            == Catch::Approx(envelope.traversalGrid.values[4 * envelope.traversalGrid.rows]));
}

TEST_CASE("Dynamic Envelope morph preparation is coalesced off the process path",
        "[cycle-v2][runtime][envelope][modulation]") {
    auto parameters = envelopeParameters();
    parameters.push_back({ "red", "Red", "0.5" });
    parameters.push_back({ "blue", "Blue", "0.5" });
    parameters.push_back({ "dynamic", "Dynamic While Live", "1" });
    const auto configuration = EnvelopeSignalProcessor::buildConfiguration(parameters);
    REQUIRE(configuration != nullptr);

    EnvelopeSignalProcessor processor;
    processor.adoptConfiguration({ 1, "dynamic-envelope", configuration });
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 16;
    spec.sampleRate = 32.0;
    processor.prepareExecution(spec);

    const auto processAt = [&](float red, float blue, bool noteOn) {
        AudioProcessContext context;
        context.frameCount = 8;
        context.timing.sampleRate = 32.0;
        context.inputs = { payload({ red }), payload({ blue }) };
        context.outputPorts = { { "env", PortDomain::EnvelopeSignal, ChannelLayout::Mono } };
        if (noteOn) {
            context.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
        }
        processor.process(context);
        return output(context).traversalGrid.values;
    };

    const auto base = processAt(0.1f, 0.2f, true);
    const double positionBeforeAdoption = processor.playbackPosition();
    REQUIRE(processor.dynamicDiagnostics().requests == 1);
    REQUIRE(processor.dynamicDiagnostics().preparations == 0);
    REQUIRE(processor.serviceNonRealtimePreparation());
    const auto firstDynamic = processAt(0.1f, 0.2f, false);
    REQUIRE(firstDynamic != base);
    REQUIRE(processor.playbackPosition() > positionBeforeAdoption);
    REQUIRE(processor.playbackMode() == Rasterization::EnvelopePlaybackMode::Normal);
    REQUIRE(processor.dynamicDiagnostics().adoptions == 1);

    processAt(0.3f, 0.4f, false);
    processAt(0.7f, 0.8f, false);
    REQUIRE(processor.dynamicDiagnostics().requests == 3);
    REQUIRE(processor.serviceNonRealtimePreparation());
    REQUIRE(processor.dynamicDiagnostics().preparations == 2);
    REQUIRE_FALSE(processor.serviceNonRealtimePreparation());
    const auto newestDynamic = processAt(0.7f, 0.8f, false);
    REQUIRE(newestDynamic != firstDynamic);
    REQUIRE(processor.dynamicDiagnostics().adoptions == 2);
}

TEST_CASE("Smoothed Envelope morph requests have bounded cadence at audio rate",
        "[cycle-v2][runtime][envelope][modulation][smoothing]") {
    auto parameters = envelopeParameters();
    parameters.push_back({ "red", "Red", "0.5" });
    parameters.push_back({ "blue", "Blue", "0.5" });
    parameters.push_back({ "dynamic", "Dynamic While Live", "1" });
    const auto configuration = EnvelopeSignalProcessor::buildConfiguration(parameters);
    REQUIRE(configuration != nullptr);

    EnvelopeSignalProcessor processor;
    processor.adoptConfiguration({ 1, "smoothed-envelope", configuration });
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 16;
    spec.sampleRate = 44100.0;
    processor.prepareExecution(spec);

    const auto processTarget = [&](bool noteOn) {
        AudioProcessContext context;
        context.frameCount = 16;
        context.timing.sampleRate = 44100.0;
        context.inputs = { payload({ 1.f }), payload({ 0.5f }) };
        context.outputPorts = { { "env", PortDomain::EnvelopeSignal, ChannelLayout::Mono } };
        if (noteOn) {
            context.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
        }
        processor.process(context);
    };

    processTarget(true);
    REQUIRE(processor.dynamicDiagnostics().requests == 1);
    processTarget(false);
    processTarget(false);
    processTarget(false);
    REQUIRE(processor.dynamicDiagnostics().requests == 1);
    processTarget(false);
    REQUIRE(processor.dynamicDiagnostics().requests == 2);
    REQUIRE(processor.serviceNonRealtimePreparation());
    REQUIRE_FALSE(processor.serviceNonRealtimePreparation());
}

TEST_CASE("Prepared Envelope adoption preserves sample continuity",
        "[cycle-v2][runtime][envelope][modulation][smoothing]") {
    auto parameters = envelopeParameters();
    parameters.push_back({ "red", "Red", "0.5" });
    parameters.push_back({ "blue", "Blue", "0.5" });
    parameters.push_back({ "dynamic", "Dynamic While Live", "1" });
    const auto configuration = EnvelopeSignalProcessor::buildConfiguration(parameters);
    REQUIRE(configuration != nullptr);

    EnvelopeSignalProcessor processor;
    processor.adoptConfiguration({ 1, "continuous-envelope", configuration });
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 64;
    spec.sampleRate = 44100.0;
    processor.prepareExecution(spec);

    AudioProcessContext first;
    first.frameCount = 64;
    first.timing.sampleRate = 44100.0;
    first.inputs = { payload({ 1.f }), payload({ 0.5f }) };
    first.outputPorts = { { "env", PortDomain::EnvelopeSignal, ChannelLayout::Mono } };
    first.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    processor.process(first);
    REQUIRE(processor.serviceNonRealtimePreparation());

    AudioProcessContext adopted = first;
    adopted.outputs.clear();
    adopted.voice.events.clear();
    processor.process(adopted);

    REQUIRE(output(adopted).block.samples.front()
            == Catch::Approx(output(first).block.samples.back()).margin(1.0e-6f));
}

TEST_CASE("Disabled dynamic Envelope latches absolute morph inputs per note",
        "[cycle-v2][runtime][envelope][modulation]") {
    auto parameters = envelopeParameters();
    parameters.push_back({ "red", "Red", "0.5" });
    parameters.push_back({ "blue", "Blue", "0.5" });
    parameters.push_back({ "dynamic", "Dynamic While Live", "0" });
    const auto configuration = EnvelopeSignalProcessor::buildConfiguration(parameters);
    REQUIRE(configuration != nullptr);

    EnvelopeSignalProcessor processor;
    processor.adoptConfiguration({ 1, "latched-envelope", configuration });
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 16;
    spec.sampleRate = 32.0;
    processor.prepareExecution(spec);

    const auto processAt = [&](float red, bool noteOn) {
        AudioProcessContext context;
        context.frameCount = 8;
        context.timing.sampleRate = 32.0;
        context.inputs = { payload({ red }), payload({ 0.25f }) };
        context.outputPorts = { { "env", PortDomain::EnvelopeSignal, ChannelLayout::Mono } };
        if (noteOn) {
            context.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
        }
        processor.process(context);
        return output(context).traversalGrid.values;
    };

    processAt(0.1f, true);
    REQUIRE(processor.serviceNonRealtimePreparation());
    const auto firstNote = processAt(0.1f, false);
    const uint64_t requestsAfterLatch = processor.dynamicDiagnostics().requests;

    const auto unchangedActiveNote = processAt(0.9f, false);
    REQUIRE(unchangedActiveNote == firstNote);
    REQUIRE(processor.dynamicDiagnostics().requests == requestsAfterLatch);
    REQUIRE_FALSE(processor.serviceNonRealtimePreparation());

    processAt(0.9f, true);
    REQUIRE(processor.dynamicDiagnostics().requests == requestsAfterLatch + 1);
    REQUIRE(processor.serviceNonRealtimePreparation());
    const auto secondNote = processAt(0.9f, false);
    REQUIRE(secondNote != firstNote);
}

TEST_CASE("Disabling live Envelope morph freezes each voice's adopted position",
        "[cycle-v2][runtime][envelope][modulation]") {
    auto liveParameters = envelopeParameters();
    liveParameters.push_back({ "red", "Red", "0.5" });
    liveParameters.push_back({ "blue", "Blue", "0.5" });
    liveParameters.push_back({ "dynamic", "Dynamic While Live", "1" });
    auto latchedParameters = liveParameters;
    for (auto& parameter : latchedParameters) {
        if (parameter.id == "dynamic") {
            parameter.value = "0";
        }
    }
    const auto live = EnvelopeSignalProcessor::buildConfiguration(liveParameters);
    const auto latched = EnvelopeSignalProcessor::buildConfiguration(latchedParameters);
    REQUIRE(live != nullptr);
    REQUIRE(latched != nullptr);

    EnvelopeSignalProcessor first;
    EnvelopeSignalProcessor second;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 8;
    spec.sampleRate = 64.0;
    for (auto* processor : { &first, &second }) {
        processor->adoptConfiguration({ 1, "live-envelope", live });
        processor->prepareExecution(spec);
    }
    const auto processAt = [](EnvelopeSignalProcessor& processor, float red, bool noteOn) {
        AudioProcessContext context;
        context.frameCount = 8;
        context.timing.sampleRate = 64.0;
        context.inputs = { payload({ red }), payload({ 0.5f }) };
        context.outputPorts = { { "env", PortDomain::EnvelopeSignal, ChannelLayout::Mono } };
        if (noteOn) {
            context.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
        }
        processor.process(context);
        return output(context).traversalGrid.values;
    };

    processAt(first, 0.1f, true);
    processAt(second, 0.9f, true);
    REQUIRE(first.serviceNonRealtimePreparation());
    REQUIRE(second.serviceNonRealtimePreparation());
    const auto firstAdopted = processAt(first, 0.1f, false);
    const auto secondAdopted = processAt(second, 0.9f, false);
    REQUIRE(firstAdopted != secondAdopted);

    first.adoptConfiguration({ 2, "latched-envelope", latched });
    first.prepareExecution(spec);
    const auto frozen = processAt(first, 0.9f, false);
    REQUIRE(frozen == firstAdopted);
    REQUIRE_FALSE(first.serviceNonRealtimePreparation());
}

TEST_CASE("Envelope processor preserves active position across snapshot edits", "[cycle-v2][runtime][envelope]") {
    const String initialSnapshot = EnvelopeMeshState::defaultSnapshot();
    EnvelopeMesh editedMesh("EditedEnvelope");
    REQUIRE(EnvelopeMeshState::apply(initialSnapshot, editedMesh));

    VertCube* editedCube = editedMesh.getCubes()[2];
    for (int i = 0; i < (int) VertCube::numVerts; ++i) {
        editedCube->getVertex(i)->values[Vertex::Amp] = 0.25f;
    }

    const String editedSnapshot = EnvelopeMeshState::serialize(editedMesh);
    editedMesh.destroy();

    NodeAudioProcessorFactory factory;
    auto activeProcessor = factory.create(AudioModuleRole::Envelope);
    AudioProcessContext start;
    start.frameCount = 8;
    start.timing.sampleRate = 16.0;
    start.parameters = envelopeParameters(initialSnapshot);
    start.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    prepareProcessor(*activeProcessor, AudioModuleRole::Envelope, start, 8, 1);
    activeProcessor->process(start);

    AudioProcessContext edited;
    edited.frameCount = 4;
    edited.timing.sampleRate = 16.0;
    edited.parameters = envelopeParameters(editedSnapshot);
    prepareProcessor(*activeProcessor, AudioModuleRole::Envelope, edited, 8, 2);
    activeProcessor->process(edited);

    auto freshProcessor = factory.create(AudioModuleRole::Envelope);
    AudioProcessContext fresh;
    fresh.frameCount = 4;
    fresh.timing.sampleRate = 16.0;
    fresh.parameters = edited.parameters;
    fresh.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    prepareProcessor(*freshProcessor, AudioModuleRole::Envelope, fresh);
    freshProcessor->process(fresh);

    REQUIRE(output(edited).block.samples.front() > output(fresh).block.samples.front());
}

TEST_CASE("Utility audio processors add and multiply inputs", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext addContext;
    addContext.frameCount = 4;
    addContext.inputs = { payload({ 1.f, 2.f, 3.f, 4.f }), payload({ 0.5f, 1.f, 1.5f, 2.f }) };
    factory.create(AudioModuleRole::Add)->process(addContext);

    REQUIRE(output(addContext).block.samples == std::vector<float> { 1.5f, 3.f, 4.5f, 6.f });

    AudioProcessContext multiplyContext;
    multiplyContext.frameCount = 4;
    multiplyContext.inputs = { payload({ 1.f, 2.f, 3.f, 4.f }), payload({ 0.5f, 1.f, 1.5f, 2.f }) };
    factory.create(AudioModuleRole::Multiply)->process(multiplyContext);

    REQUIRE(output(multiplyContext).block.samples == std::vector<float> { 0.5f, 2.f, 4.5f, 8.f });
}

TEST_CASE("Utility audio processors transform traversal grids", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext addContext;
    addContext.frameCount = 3;
    addContext.inputs = {
            gridPayload({ 1.f, 2.f, 3.f, 4.f, 5.f, 6.f }, 2, 3),
            payload({ 10.f, 20.f, 30.f })
    };
    factory.create(AudioModuleRole::Add)->process(addContext);

    REQUIRE(output(addContext).traversalGrid.isValid());
    REQUIRE(output(addContext).traversalGrid.columns == 2);
    REQUIRE(output(addContext).traversalGrid.rows == 3);
    REQUIRE(output(addContext).traversalGrid.values
            == std::vector<float> { 11.f, 22.f, 33.f, 14.f, 25.f, 36.f });

    AudioProcessContext multiplyContext;
    multiplyContext.frameCount = 3;
    multiplyContext.inputs = {
            gridPayload({ 1.f, 2.f, 3.f, 4.f, 5.f, 6.f }, 2, 3),
            payload({ 2.f, 3.f, 4.f })
    };
    factory.create(AudioModuleRole::Multiply)->process(multiplyContext);

    REQUIRE(output(multiplyContext).traversalGrid.isValid());
    REQUIRE(output(multiplyContext).traversalGrid.values
            == std::vector<float> { 2.f, 6.f, 12.f, 8.f, 15.f, 24.f });
}

TEST_CASE("Utility audio processors apply scalar operands across vectors and traversal grids", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext vectorContext;
    vectorContext.frameCount = 3;
    vectorContext.inputs = {
            payload({ 2.f }),
            payload({ 1.f, 2.f, 3.f })
    };
    factory.create(AudioModuleRole::Multiply)->process(vectorContext);

    REQUIRE(output(vectorContext).block.samples == std::vector<float> { 2.f, 4.f, 6.f });

    AudioProcessContext gridContext;
    gridContext.frameCount = 3;
    gridContext.inputs = {
            gridPayload({ 1.f, 2.f, 3.f, 4.f, 5.f, 6.f }, 2, 3),
            payload({ 10.f })
    };
    factory.create(AudioModuleRole::Add)->process(gridContext);

    REQUIRE(output(gridContext).block.samples == std::vector<float> { 11.f, 12.f, 13.f });
    REQUIRE(output(gridContext).traversalGrid.values
            == std::vector<float> { 11.f, 12.f, 13.f, 14.f, 15.f, 16.f });
}

TEST_CASE("Utility audio processors broadcast single-column matrix operands", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = {
            gridPayload({ 1.f, 2.f, 3.f, 4.f, 5.f, 6.f }, 2, 3),
            gridPayload({ 10.f, 20.f, 30.f }, 1, 3)
    };
    factory.create(AudioModuleRole::Add)->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { 11.f, 22.f, 33.f });
    REQUIRE(output(context).traversalGrid.values
            == std::vector<float> { 11.f, 22.f, 33.f, 14.f, 25.f, 36.f });
}

TEST_CASE("Utility audio processors preserve output traversal metadata across elementwise grids", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    auto left = gridPayload({ 1.f, 2.f, 3.f, 4.f, 5.f, 6.f }, 2, 3);
    auto right = gridPayload({ 10.f, 20.f, 30.f, 40.f, 50.f, 60.f }, 2, 3);
    right.domain = PortDomain::ControlSignal;
    right.traversalGrid.metadata.valueDomain = PortDomain::ControlSignal;
    right.traversalGrid.metadata.rowAxis = TraversalGridAxis::Frequency;

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = { left, right };
    factory.create(AudioModuleRole::Add)->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { 11.f, 22.f, 33.f });
    REQUIRE(output(context).traversalGrid.isValid());
    REQUIRE(output(context).traversalGrid.values
            == std::vector<float> { 11.f, 22.f, 33.f, 44.f, 55.f, 66.f });
    REQUIRE(output(context).traversalGrid.metadata.rowAxis == TraversalGridAxis::Time);
}

TEST_CASE("Utility audio processors reject mismatched concrete traversal axes", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    auto left = gridPayload({ 1.f, 2.f, 3.f, 4.f, 5.f, 6.f }, 2, 3);
    auto right = gridPayload({ 10.f, 20.f, 30.f, 40.f, 50.f, 60.f }, 2, 3);
    right.traversalGrid.metadata.rowAxis = TraversalGridAxis::Frequency;

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = { left, right };
    factory.create(AudioModuleRole::Add)->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { 11.f, 22.f, 33.f });
    REQUIRE_FALSE(output(context).traversalGrid.isValid());
}

TEST_CASE("Audio process work arena reserves block and traversal-grid storage", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::WaveSource);
    REQUIRE(processor != nullptr);

    AudioProcessWorkArena arena;
    arena.prepare(8, 1, 1, 64);

    AudioProcessContext context;
    context.frameCount = 8;
    context.workArena = &arena;
    context.outputPorts = {
            { "out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo }
    };
    processor->process(context);

    REQUIRE(output(context).block.samples.capacity() >= 8);
    REQUIRE(output(context).traversalGrid.values.capacity() >= 64);
}

TEST_CASE("Transparent audio processors pass through first input", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::Output);
    REQUIRE(processor != nullptr);

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = { payload({ -0.25f, 0.f, 0.5f }) };
    processor->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { -0.25f, 0.f, 0.5f });
    REQUIRE_FALSE(output(context).traversalGrid.isValid());
}

TEST_CASE("Passthrough audio processors expand scalar input safely", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 4;
    context.inputs = { payload({ 0.25f }) };
    factory.create(AudioModuleRole::Output)->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { 0.25f, 0.25f, 0.25f, 0.25f });
}

TEST_CASE("Stereo split expands scalar input safely", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = { payload({ -0.5f }) };
    factory.create(AudioModuleRole::StereoSplit)->process(context);

    REQUIRE(context.outputs.size() == 2);
    REQUIRE(context.outputs[0].block.samples == std::vector<float> { -0.5f, -0.5f, -0.5f });
    REQUIRE(context.outputs[1].block.samples == std::vector<float> { -0.5f, -0.5f, -0.5f });
}

TEST_CASE("Waveshaper processor applies the same transform to block and traversal grid", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = {
            gridPayload({ -0.5f, 0.f, 0.5f, 0.75f, -0.75f, 0.25f }, 2, 3)
    };
    context.parameters = {
            { "pre", "Pre", "2" },
            { "post", "Post", "0.5" }
    };
    auto processor = factory.create(AudioModuleRole::Waveshaper);
    prepareProcessor(*processor, AudioModuleRole::Waveshaper, context);
    processor->process(context);

    REQUIRE(output(context).traversalGrid.isValid());
    REQUIRE(output(context).traversalGrid.columns == 2);
    REQUIRE(output(context).traversalGrid.rows == 3);
    REQUIRE(output(context).block.samples.size() == 3);
    REQUIRE(output(context).traversalGrid.values.size() == 6);
    REQUIRE(output(context).block.samples[0] == Catch::Approx(output(context).traversalGrid.values[0]));
    REQUIRE(output(context).block.samples[1] == Catch::Approx(output(context).traversalGrid.values[1]));
    REQUIRE(output(context).block.samples[2] == Catch::Approx(output(context).traversalGrid.values[2]));
    REQUIRE(output(context).block.samples[0] < 0.f);
    REQUIRE(output(context).block.samples[1] == Catch::Approx(0.f).margin(0.0001f));
    REQUIRE(output(context).block.samples[2] > 0.f);
}

TEST_CASE("Unprepared configured effects bypass without realtime reconstruction", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = { payload({ -0.5f, 0.f, 0.5f }) };
    context.parameters = {
            { "pre", "Pre", "1" },
            { "post", "Post", "0" }
    };

    factory.create(AudioModuleRole::Waveshaper)->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { -0.5f, 0.f, 0.5f });
}

TEST_CASE("Waveshaper processor uses persisted FX rasterizer vertices", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    const auto shapedModel = curveModel({
            { 1, 0.0625f, 0.875f, 1.f },
            { 2, 0.9375f, 0.875f, 1.f }
    });

    AudioProcessContext defaultContext;
    defaultContext.frameCount = 3;
    defaultContext.inputs = {
            gridPayload({ -0.5f, 0.f, 0.5f }, 1, 3)
    };
    auto defaultProcessor = factory.create(AudioModuleRole::Waveshaper);
    prepareProcessor(*defaultProcessor, AudioModuleRole::Waveshaper, defaultContext);
    defaultProcessor->process(defaultContext);

    AudioProcessContext shapedContext;
    shapedContext.frameCount = 3;
    shapedContext.inputs = {
            gridPayload({ -0.5f, 0.f, 0.5f }, 1, 3)
    };
    auto shapedProcessor = factory.create(AudioModuleRole::Waveshaper);
    prepareProcessor(*shapedProcessor, AudioModuleRole::Waveshaper, shapedContext, 0, 1, shapedModel);
    shapedProcessor->process(shapedContext);

    REQUIRE(output(shapedContext).traversalGrid.isValid());
    REQUIRE(output(shapedContext).block.samples[2] != Catch::Approx(output(defaultContext).block.samples[2]));
    REQUIRE(output(shapedContext).traversalGrid.values[1] == Catch::Approx(output(shapedContext).block.samples[1]));
}

TEST_CASE("Waveshaper oversamples audio while keeping traversal columns independent", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    AudioProcessContext context;
    context.frameCount = 64;
    std::vector<float> signal(64);
    for (size_t i = 0; i < signal.size(); ++i) {
        signal[i] = i % 2 == 0 ? -0.9f : 0.9f;
    }
    context.inputs = { gridPayload(signal, 1, signal.size()) };
    context.parameters = curveParameters({
            { 1, 0.0625f, 0.95f, 1.f },
            { 2, 0.9375f, 0.05f, 1.f }
    });
    context.parameters.insert(context.parameters.end(), {
            { "pre", "Pre", "2" },
            { "post", "Post", "1" },
            { "aaFactor", "AA Factor", "4" }
    });

    auto processor = factory.create(AudioModuleRole::Waveshaper);
    prepareProcessor(*processor, AudioModuleRole::Waveshaper, context);
    processor->process(context);

    REQUIRE(output(context).traversalGrid.isValid());
    REQUIRE(output(context).block.samples != output(context).traversalGrid.values);
    REQUIRE(std::all_of(
            output(context).block.samples.begin(),
            output(context).block.samples.end(),
            [](float value) { return std::isfinite(value); }));
}

TEST_CASE("Disabled waveshaper passes block and traversal grid through unchanged", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = {
            gridPayload({ -0.5f, 0.f, 0.5f, 0.75f, -0.75f, 0.25f }, 2, 3)
    };
    context.parameters = {
            { "enabled", "Enabled", "0" },
            { "pre", "Pre", "2" },
            { "post", "Post", "0.5" }
    };
    auto processor = factory.create(AudioModuleRole::Waveshaper);
    prepareProcessor(*processor, AudioModuleRole::Waveshaper, context);
    processor->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { -0.5f, 0.f, 0.5f });
    REQUIRE(output(context).traversalGrid.values
            == std::vector<float> { -0.5f, 0.f, 0.5f, 0.75f, -0.75f, 0.25f });
}

TEST_CASE("IR processor transforms block and traversal grid through convolution", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 4;
    context.inputs = {
            gridPayload({ 1.f, 0.f, 0.f, 0.f, 0.5f, 0.f, 0.f, 0.f }, 2, 4)
    };
    context.parameters = {
            { "size", "Size", "0" },
            { "post", "Post", "0.5" },
            { "highPass", "HighPass", "0" }
    };
    auto processor = factory.create(AudioModuleRole::ImpulseResponse);
    prepareProcessor(*processor, AudioModuleRole::ImpulseResponse, context);
    processor->process(context);

    REQUIRE(output(context).traversalGrid.isValid());
    REQUIRE(output(context).traversalGrid.columns == 2);
    REQUIRE(output(context).traversalGrid.rows == 4);
    REQUIRE(output(context).block.samples != std::vector<float> { 1.f, 0.f, 0.f, 0.f });
    REQUIRE(output(context).traversalGrid.values != context.inputs.front().traversalGrid.values);
    for (size_t row = 0; row < context.frameCount; ++row) {
        REQUIRE(output(context).traversalGrid.values[row]
                == Catch::Approx(output(context).block.samples[row]).margin(1.0e-5f));
    }
}

TEST_CASE("IR processor uses Cycle 1 post-gain and prefilter policies", "[cycle-v2][runtime][ir]") {
    NodeAudioProcessorFactory factory;
    auto unityProcessor = factory.create(AudioModuleRole::ImpulseResponse);
    auto quietProcessor = factory.create(AudioModuleRole::ImpulseResponse);
    auto filteredProcessor = factory.create(AudioModuleRole::ImpulseResponse);
    std::vector<float> input(128, 0.f);
    input[0] = 1.f;

    AudioProcessContext unity;
    unity.frameCount = input.size();
    unity.inputs = { payload(input) };
    unity.parameters = curveParameters({
            { 1, 0.03125f, 0.75f, 1.f },
            { 2, 0.0625f, 0.75f, 1.f },
            { 3, 1.f, 0.75f, 1.f }
    });
    unity.parameters.insert(unity.parameters.end(), {
            { "size", "Size", "0" },
            { "post", "Post", "0.5" },
            { "highPass", "HighPass", "0" }
    });
    prepareProcessor(*unityProcessor, AudioModuleRole::ImpulseResponse, unity);
    unityProcessor->process(unity);

    AudioProcessContext quiet = unity;
    std::find_if(quiet.parameters.begin(), quiet.parameters.end(), [](const auto& parameter) {
        return parameter.id == "post";
    })->value = "0";
    prepareProcessor(*quietProcessor, AudioModuleRole::ImpulseResponse, quiet);
    quietProcessor->process(quiet);

    AudioProcessContext filtered = unity;
    std::find_if(filtered.parameters.begin(), filtered.parameters.end(), [](const auto& parameter) {
        return parameter.id == "highPass";
    })->value = "1";
    prepareProcessor(*filteredProcessor, AudioModuleRole::ImpulseResponse, filtered);
    filteredProcessor->process(filtered);

    const auto& unitySamples = output(unity).block.samples;
    const auto& quietSamples = output(quiet).block.samples;
    const auto& filteredSamples = output(filtered).block.samples;
    const float unityMagnitude = std::accumulate(
            unitySamples.begin(), unitySamples.end(), 0.f,
            [](float sum, float value) { return sum + std::abs(value); });
    const float quietMagnitude = std::accumulate(
            quietSamples.begin(), quietSamples.end(), 0.f,
            [](float sum, float value) { return sum + std::abs(value); });
    REQUIRE(quietMagnitude / unityMagnitude == Catch::Approx(std::exp(-5.f)).epsilon(0.02f));
    REQUIRE(filteredSamples != unitySamples);
}

TEST_CASE("Disabled IR processor passes its input through", "[cycle-v2][runtime][ir]") {
    NodeAudioProcessorFactory factory;
    AudioProcessContext context;
    context.frameCount = 4;
    context.inputs = { payload({ 1.f, -0.5f, 0.25f, 0.f }) };
    context.parameters = {
            { "enabled", "Enabled", "0" },
            { "size", "Size", "1" },
            { "post", "Post", "1" },
            { "highPass", "HighPass", "1" }
    };

    auto processor = factory.create(AudioModuleRole::ImpulseResponse);
    prepareProcessor(*processor, AudioModuleRole::ImpulseResponse, context);
    processor->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { 1.f, -0.5f, 0.25f, 0.f });
}

TEST_CASE("IR processor preserves the graph channel policy", "[cycle-v2][runtime][ir]") {
    NodeAudioProcessorFactory factory;
    std::vector<NodeParameter> parameters = curveParameters({
            { 1, 0.f, 0.5f, 1.f },
            { 2, 0.0625f, 0.95f, 0.35f },
            { 3, 0.125f, 0.3f, 0.45f },
            { 4, 0.1875f, 0.55f, 0.7f },
            { 5, 1.f, 0.5f, 1.f }
    });
    parameters.insert(parameters.end(), {
            { "size", "Size", "0" },
            { "post", "Post", "0.5" },
            { "highPass", "HighPass", "0" }
    });
    std::vector<float> reference;

    for (ChannelLayout layout : {
            ChannelLayout::Mono,
            ChannelLayout::LinkedStereo,
            ChannelLayout::Left,
            ChannelLayout::Right,
            ChannelLayout::StereoPair }) {
        AudioProcessContext context;
        context.frameCount = 64;
        context.inputs = { payload(std::vector<float>(64, 0.f)) };
        context.inputs.front().block.samples.front() = 1.f;
        context.inputs.front().channelLayout = layout;
        context.parameters = parameters;

        auto processor = factory.create(AudioModuleRole::ImpulseResponse);
        prepareProcessor(*processor, AudioModuleRole::ImpulseResponse, context);
        processor->process(context);

        REQUIRE(output(context).channelLayout == layout);
        if (reference.empty()) {
            reference = output(context).block.samples;
        } else {
            REQUIRE(output(context).block.samples == reference);
        }
    }
}

TEST_CASE("IR processor is split-block equivalent and preserves its tail", "[cycle-v2][runtime][ir]") {
    NodeAudioProcessorFactory factory;
    auto wholeProcessor = factory.create(AudioModuleRole::ImpulseResponse);
    auto splitProcessor = factory.create(AudioModuleRole::ImpulseResponse);
    std::vector<NodeParameter> parameters = curveParameters({
            { 1, 0.f, 0.5f, 1.f },
            { 2, 0.0625f, 0.95f, 0.35f },
            { 3, 0.125f, 0.3f, 0.45f },
            { 4, 0.1875f, 0.55f, 0.7f },
            { 5, 1.f, 0.5f, 1.f }
    });
    const auto model = curveModel({
            { 1, 0.f, 0.5f, 1.f },
            { 2, 0.0625f, 0.95f, 0.35f },
            { 3, 0.125f, 0.3f, 0.45f },
            { 4, 0.1875f, 0.55f, 0.7f },
            { 5, 1.f, 0.5f, 1.f }
    });
    parameters.insert(parameters.end(), {
            { "size", "Size", "0" },
            { "post", "Post", "0.5" },
            { "highPass", "HighPass", "0" }
    });
    std::vector<float> wholeInput(128, 0.f);
    wholeInput[0] = 1.f;

    AudioProcessContext whole;
    whole.frameCount = wholeInput.size();
    whole.inputs = { payload(wholeInput) };
    whole.parameters = parameters;
    prepareProcessor(*wholeProcessor, AudioModuleRole::ImpulseResponse, whole, 0, 1, model);
    wholeProcessor->process(whole);

    AudioProcessContext firstHalf;
    firstHalf.frameCount = 64;
    firstHalf.inputs = { payload(std::vector<float>(wholeInput.begin(), wholeInput.begin() + 64)) };
    firstHalf.parameters = parameters;
    prepareProcessor(*splitProcessor, AudioModuleRole::ImpulseResponse, firstHalf, 0, 1, model);
    splitProcessor->process(firstHalf);

    AudioProcessContext secondHalf;
    secondHalf.frameCount = 64;
    secondHalf.inputs = { payload(std::vector<float>(64, 0.f)) };
    secondHalf.parameters = parameters;
    splitProcessor->process(secondHalf);

    const auto& wholeSamples = output(whole).block.samples;
    const auto& firstHalfSamples = output(firstHalf).block.samples;
    const auto& secondHalfSamples = output(secondHalf).block.samples;
    for (size_t i = 0; i < 64; ++i) {
        REQUIRE(firstHalfSamples[i] == Catch::Approx(wholeSamples[i]).margin(1.0e-5f));
        REQUIRE(secondHalfSamples[i] == Catch::Approx(wholeSamples[i + 64]).margin(1.0e-5f));
    }
    REQUIRE(std::any_of(
            secondHalfSamples.begin(),
            secondHalfSamples.end(),
            [](float value) { return std::abs(value) > 1.0e-6f; }));
}

TEST_CASE("Delay processor transforms block and traversal grid with matching delay state", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 8;
    context.timing.sampleRate = 128.0;
    context.inputs = {
            gridPayload(
                    {
                            1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f,
                            10.f, 20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 80.f
                    },
                    2,
                    8)
    };
    context.parameters = {
            { "time", "Time", "0" },
            { "feedback", "Feedback", "1" },
            { "wet", "Wet", "1" }
    };
    factory.create(AudioModuleRole::Delay)->process(context);

    REQUIRE(output(context).block.samples[0] == Catch::Approx(1.f));
    REQUIRE(output(context).block.samples[5] > context.inputs.front().block.samples[5]);
    REQUIRE(output(context).traversalGrid.values[0] == Catch::Approx(output(context).block.samples[0]));
    REQUIRE(output(context).traversalGrid.values[5] == Catch::Approx(output(context).block.samples[5]));
    REQUIRE(output(context).traversalGrid.values[8] > context.inputs.front().traversalGrid.values[8]);
}

TEST_CASE("Delay traversal rendering does not overwrite block state", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto withGridProcessor = factory.create(AudioModuleRole::Delay);
    auto blockOnlyProcessor = factory.create(AudioModuleRole::Delay);

    const std::vector<NodeParameter> parameters = {
            { "time", "Time", "0" },
            { "feedback", "Feedback", "1" },
            { "wet", "Wet", "1" }
    };

    AudioProcessContext withGridFirst;
    withGridFirst.frameCount = 8;
    withGridFirst.timing.sampleRate = 128.0;
    withGridFirst.parameters = parameters;
    withGridFirst.inputs = {
            gridPayload(
                    {
                            1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f,
                            100.f, 200.f, 300.f, 400.f, 500.f, 600.f, 700.f, 800.f
                    },
                    2,
                    8)
    };
    withGridProcessor->process(withGridFirst);

    AudioProcessContext blockOnlyFirst;
    blockOnlyFirst.frameCount = 8;
    blockOnlyFirst.timing.sampleRate = 128.0;
    blockOnlyFirst.parameters = parameters;
    blockOnlyFirst.inputs = {
            payload({ 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f })
    };
    blockOnlyProcessor->process(blockOnlyFirst);

    AudioProcessContext withGridSecond;
    withGridSecond.frameCount = 8;
    withGridSecond.timing.sampleRate = 128.0;
    withGridSecond.parameters = parameters;
    withGridSecond.inputs = { payload({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }) };
    withGridProcessor->process(withGridSecond);

    AudioProcessContext blockOnlySecond = withGridSecond;
    blockOnlyProcessor->process(blockOnlySecond);

    REQUIRE(output(withGridSecond).block.samples == output(blockOnlySecond).block.samples);
    REQUIRE(output(withGridSecond).block.samples[0] > 0.f);
}

TEST_CASE("Reverb processor transforms block and carries traversal tails across columns", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 256;
    std::vector<float> values(512, 0.f);
    values.front() = 1.f;
    context.inputs = {
            gridPayload(values, 2, 256)
    };
    context.parameters = {
            { "size", "Size", "0" },
            { "damp", "Damp", "0.2" },
            { "highPass", "HighPass", "0" },
            { "width", "Width", "0.5" },
            { "wet", "Wet", "1" }
    };
    auto processor = factory.create(AudioModuleRole::Reverb);
    prepareProcessor(*processor, AudioModuleRole::Reverb, context);
    processor->process(context);

    REQUIRE(output(context).traversalGrid.isValid());
    REQUIRE(output(context).block.samples != context.inputs.front().block.samples);
    REQUIRE(output(context).traversalGrid.values != context.inputs.front().traversalGrid.values);
    REQUIRE(std::any_of(
            output(context).traversalGrid.values.begin() + 256,
            output(context).traversalGrid.values.end(),
            [](float value) { return std::abs(value) > 0.000001f; }));
}

TEST_CASE("Reverb width mixes independent stereo channels", "[cycle-v2][runtime][reverb][stereo]") {
    NodeAudioProcessorFactory factory;
    AudioProcessContext context;
    context.frameCount = 256;
    context.parameters = {
            { "size", "Size", "0" },
            { "damp", "Damp", "0.2" },
            { "highPass", "High Pass", "0" },
            { "width", "Width", "1" },
            { "wet", "Wet", "1" }
    };
    SignalPayload stereo = payload(std::vector<float>(256, 0.f));
    stereo.channelLayout = ChannelLayout::StereoPair;
    stereo.secondaryBlock.samples.resize(256);
    stereo.block.samples.front() = 1.f;
    context.inputs = { stereo };

    auto processor = factory.create(AudioModuleRole::Reverb);
    prepareProcessor(*processor, AudioModuleRole::Reverb, context);
    processor->process(context);

    REQUIRE(output(context).isStereo());
    REQUIRE(output(context).secondaryBlock.samples.size() == 256);
    REQUIRE(output(context).block.samples != output(context).secondaryBlock.samples);
}

TEST_CASE("Equalizer processor applies the published five-band response", "[cycle-v2][runtime][equalizer]") {
    NodeAudioProcessorFactory factory;
    AudioProcessContext context;
    context.frameCount = 512;
    context.timing.sampleRate = 44100.0;
    context.inputs = { payload(std::vector<float>(512, 1.f)) };
    context.parameters = {
            { "band1Gain", "Band 1 Gain", "1" },
            { "band1Frequency", "Band 1 Frequency", "0.2" }
    };
    auto processor = factory.create(AudioModuleRole::Equalizer);
    prepareProcessor(*processor, AudioModuleRole::Equalizer, context);
    processor->process(context);

    REQUIRE(output(context).block.samples.size() == 512);
    REQUIRE(output(context).block.samples != context.inputs.front().block.samples);
}

TEST_CASE("Reverb traversal state restarts for each grid render", "[cycle-v2][runtime][reverb]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::Reverb);

    AudioProcessContext first;
    first.frameCount = 256;
    std::vector<float> values(512, 0.f);
    values.front() = 1.f;
    first.inputs = { gridPayload(values, 2, 256) };
    first.parameters = {
            { "size", "Size", "0" },
            { "damp", "Damp", "0.2" },
            { "highPass", "HighPass", "0" },
            { "width", "Width", "0.5" },
            { "wet", "Wet", "1" }
    };
    prepareProcessor(*processor, AudioModuleRole::Reverb, first);
    processor->process(first);
    const auto firstGrid = output(first).traversalGrid.values;

    AudioProcessContext second;
    second.frameCount = first.frameCount;
    second.inputs = first.inputs;
    second.parameters = first.parameters;
    processor->process(second);

    REQUIRE(output(second).traversalGrid.values == firstGrid);
}

TEST_CASE("Reverb traversal rendering does not overwrite block state", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto withGridProcessor = factory.create(AudioModuleRole::Reverb);
    auto blockOnlyProcessor = factory.create(AudioModuleRole::Reverb);
    const std::vector<NodeParameter> parameters = {
            { "size", "Size", "0" },
            { "damp", "Damp", "0.2" },
            { "highPass", "HighPass", "0" },
            { "width", "Width", "0.5" },
            { "wet", "Wet", "1" }
    };

    AudioProcessContext withGridFirst;
    withGridFirst.frameCount = 256;
    withGridFirst.parameters = parameters;
    std::vector<float> gridValues(512, 0.f);
    gridValues.front() = 1.f;
    gridValues[256] = 100.f;
    withGridFirst.inputs = { gridPayload(gridValues, 2, 256) };
    prepareProcessor(*withGridProcessor, AudioModuleRole::Reverb, withGridFirst);
    withGridProcessor->process(withGridFirst);

    AudioProcessContext blockOnlyFirst;
    blockOnlyFirst.frameCount = 256;
    blockOnlyFirst.parameters = parameters;
    std::vector<float> impulse(256, 0.f);
    impulse.front() = 1.f;
    blockOnlyFirst.inputs = { payload(impulse) };
    prepareProcessor(*blockOnlyProcessor, AudioModuleRole::Reverb, blockOnlyFirst);
    blockOnlyProcessor->process(blockOnlyFirst);

    AudioProcessContext withGridSecond;
    withGridSecond.frameCount = 256;
    withGridSecond.parameters = parameters;
    withGridSecond.inputs = { payload(std::vector<float>(256, 0.f)) };
    withGridProcessor->process(withGridSecond);

    AudioProcessContext blockOnlySecond = withGridSecond;
    blockOnlyProcessor->process(blockOnlySecond);

    REQUIRE(output(withGridSecond).block.samples.size()
            == output(blockOnlySecond).block.samples.size());
    for (size_t i = 0; i < output(withGridSecond).block.samples.size(); ++i) {
        REQUIRE(output(withGridSecond).block.samples[i]
                == Catch::Approx(output(blockOnlySecond).block.samples[i]).margin(0.000001f));
    }
}

TEST_CASE("FFT cycle processor publishes separate magnitude and phase ports", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::Fft);
    REQUIRE(processor != nullptr);

    AudioProcessContext context;
    context.frameCount = 4;
    context.inputs = { gridPayload({ -0.5f, -0.25f, 0.25f, 0.5f }, 1, 4) };
    context.outputPorts = {
            { "mag", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo },
            { "phase", PortDomain::SpectralPhaseSignal, ChannelLayout::LinkedStereo }
    };
    processor->process(context);

    REQUIRE(context.outputs.size() == 2);
    REQUIRE(context.outputs[0].domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(context.outputs[0].block.samples.size() == 3);
    REQUIRE(std::any_of(
            context.outputs[0].block.samples.begin(),
            context.outputs[0].block.samples.end(),
            [](float value) {
                return value > 0.f;
            }));
    REQUIRE(context.outputs[0].traversalGrid.isValid());
    REQUIRE(context.outputs[0].traversalGrid.metadata.valueDomain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(context.outputs[0].traversalGrid.metadata.rowAxis == TraversalGridAxis::Frequency);
    REQUIRE(context.outputs[1].domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(context.outputs[1].block.samples.size() == 3);
    REQUIRE(context.outputs[1].traversalGrid.isValid());
    REQUIRE(context.outputs[1].traversalGrid.metadata.valueDomain == PortDomain::SpectralPhaseSignal);
    REQUIRE(context.outputs[1].traversalGrid.metadata.rowAxis == TraversalGridAxis::Frequency);
}

TEST_CASE("FFT and IFFT cycle processors round trip zero-mean cycle buffers", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;
    const std::vector<float> timeColumns {
            -0.5f, -0.25f, 0.25f, 0.5f,
            0.75f, -0.75f, -0.25f, 0.25f
    };

    AudioProcessContext fftContext;
    fftContext.frameCount = 4;
    fftContext.inputs = { gridPayload(timeColumns, 2, 4) };
    fftContext.inputs.front().traversalGrid.metadata.columnResolution = {
            0.25,
            0.125,
            "seconds"
    };
    fftContext.outputPorts = {
            { "mag", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo },
            { "phase", PortDomain::SpectralPhaseSignal, ChannelLayout::LinkedStereo }
    };
    factory.create(AudioModuleRole::Fft)->process(fftContext);

    REQUIRE(fftContext.outputs[0].traversalGrid.columns == 2);
    REQUIRE(fftContext.outputs[0].traversalGrid.rows == 3);
    REQUIRE(fftContext.outputs[0].traversalGrid.metadata.columnResolution.origin == 0.25);
    REQUIRE(fftContext.outputs[0].traversalGrid.metadata.columnResolution.step == 0.125);
    REQUIRE(fftContext.outputs[0].traversalGrid.metadata.columnResolution.unit == "seconds");
    REQUIRE(fftContext.outputs[1].traversalGrid.columns == 2);
    REQUIRE(fftContext.outputs[1].traversalGrid.rows == 3);

    AudioProcessContext ifftContext;
    ifftContext.frameCount = 4;
    ifftContext.inputs = { fftContext.outputs[0], fftContext.outputs[1] };
    ifftContext.outputPorts = {
            { "time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo }
    };
    factory.create(AudioModuleRole::Ifft)->process(ifftContext);

    REQUIRE(output(ifftContext).traversalGrid.metadata.valueDomain == PortDomain::TimeSignal);
    REQUIRE(output(ifftContext).traversalGrid.metadata.rowAxis == TraversalGridAxis::Time);
    REQUIRE(output(ifftContext).traversalGrid.columns == 2);
    REQUIRE(output(ifftContext).traversalGrid.rows == 4);
    REQUIRE(output(ifftContext).traversalGrid.metadata.columnResolution.origin == 0.25);
    REQUIRE(output(ifftContext).traversalGrid.metadata.columnResolution.step == 0.125);
    REQUIRE(output(ifftContext).traversalGrid.metadata.columnResolution.unit == "seconds");
    for (size_t sample = 0; sample < timeColumns.size(); ++sample) {
        REQUIRE(output(ifftContext).traversalGrid.values[sample]
                == Catch::Approx(timeColumns[sample]).margin(1.0e-5f));
    }
}

TEST_CASE("IFFT rejects incompatible phase traversal columns", "[cycle-v2][runtime][fft]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext fftContext;
    fftContext.frameCount = 4;
    fftContext.inputs = {
            gridPayload({
                    -0.5f, -0.25f, 0.25f, 0.5f,
                    0.75f, -0.75f, -0.25f, 0.25f
            }, 2, 4)
    };
    fftContext.outputPorts = {
            { "mag", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo },
            { "phase", PortDomain::SpectralPhaseSignal, ChannelLayout::LinkedStereo }
    };
    factory.create(AudioModuleRole::Fft)->process(fftContext);

    AudioProcessContext magnitudeOnly;
    magnitudeOnly.frameCount = 4;
    magnitudeOnly.inputs = { fftContext.outputs[0] };
    factory.create(AudioModuleRole::Ifft)->process(magnitudeOnly);

    auto incompatiblePhase = fftContext.outputs[1];
    incompatiblePhase.traversalGrid.metadata.rowAxis = TraversalGridAxis::Time;
    AudioProcessContext incompatible;
    incompatible.frameCount = 4;
    incompatible.inputs = { fftContext.outputs[0], incompatiblePhase };
    factory.create(AudioModuleRole::Ifft)->process(incompatible);

    REQUIRE(output(incompatible).traversalGrid.values
            == output(magnitudeOnly).traversalGrid.values);
}

TEST_CASE("Mesh source processor generates a deterministic operand when unconnected", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 4;
    context.outputPorts = {
            { "out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo }
    };
    factory.create(AudioModuleRole::MeshSource)->process(context);

    REQUIRE(output(context).domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(output(context).block.samples.size() == 4);
    REQUIRE(output(context).traversalGrid.isValid());
    REQUIRE(output(context).traversalGrid.columns == 8);
    REQUIRE(output(context).traversalGrid.rows == 3);
    REQUIRE(*std::min_element(output(context).block.samples.begin(), output(context).block.samples.end())
            < *std::max_element(output(context).block.samples.begin(), output(context).block.samples.end()));
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

    REQUIRE(output(timeContext).domain == PortDomain::TimeSignal);
    REQUIRE(output(spectralContext).domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(output(timeContext).block.samples != output(spectralContext).block.samples);
}

TEST_CASE("Mesh source morph inputs are absolute and override persisted positions",
        "[cycle-v2][runtime][modulation]") {
    NodeAudioProcessorFactory factory;
    auto baseProcessor = factory.create(AudioModuleRole::MeshSource);
    auto modulatedProcessor = factory.create(AudioModuleRole::MeshSource);

    AudioProcessContext base;
    base.frameCount = 16;
    base.parameters = {
            { "yellow", "Yellow", "0.5" },
            { "red", "Red", "0.5" },
            { "blue", "Blue", "0.5" }
    };
    base.outputPorts = { { "out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo } };
    baseProcessor->process(base);

    AudioProcessContext modulated;
    modulated.frameCount = 16;
    modulated.parameters = base.parameters;
    modulated.inputs.resize(5);
    modulated.inputs[2] = payload({ 0.1f });
    modulated.inputs[3] = payload({ 0.9f });
    modulated.inputs[4] = payload({ 0.2f });
    modulated.outputPorts = base.outputPorts;
    modulatedProcessor->process(modulated);
    const auto firstSmoothedBlock = output(modulated).block.samples;

    REQUIRE(firstSmoothedBlock != output(base).block.samples);

    AudioProcessContext continued = modulated;
    continued.outputs.clear();
    modulatedProcessor->process(continued);
    REQUIRE(output(continued).block.samples != firstSmoothedBlock);

    AudioProcessContext clamped = modulated;
    clamped.outputs.clear();
    clamped.inputs[2] = payload({ -1.f });
    clamped.inputs[3] = payload({ 2.f });
    clamped.inputs[4] = payload({ 0.2f });
    modulatedProcessor->process(clamped);
    REQUIRE(output(clamped).block.samples != output(modulated).block.samples);
}

TEST_CASE("Mesh source processor defensively ignores unexpected signal inputs", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = {
            payload({ 0.f, 0.f, 0.f }),
            payload({ 0.25f, 0.5f, 0.75f })
    };
    context.inputs[0].domain = PortDomain::DomainContext;
    context.inputs[0].channelLayout = ChannelLayout::Mono;
    context.outputPorts = {
            { "out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo }
    };
    factory.create(AudioModuleRole::MeshSource)->process(context);

    REQUIRE(output(context).block.samples.size() == 3);
    REQUIRE(*std::min_element(output(context).block.samples.begin(), output(context).block.samples.end())
            < *std::max_element(output(context).block.samples.begin(), output(context).block.samples.end()));
}
