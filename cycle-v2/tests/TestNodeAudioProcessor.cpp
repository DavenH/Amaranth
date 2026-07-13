#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Runtime/AudioProcessContextUtils.h"
#include "../src/Runtime/NodeAudioProcessor.h"
#include "../src/Nodes/Envelope/EnvelopeMeshState.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace CycleV2;

namespace {

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
    envelopeContext.parameters = {
            { EnvelopeMeshState::parameterId(), "Envelope Snapshot", EnvelopeMeshState::defaultSnapshot() }
    };
    envelopeContext.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    factory.create(AudioModuleRole::Envelope)->process(envelopeContext);

    REQUIRE(output(envelopeContext).block.samples.front() < 0.001f);
    REQUIRE(output(envelopeContext).block.samples.back() > output(envelopeContext).block.samples.front());
}

TEST_CASE("Envelope processor applies sample-offset lifecycle events", "[cycle-v2][runtime][envelope]") {
    NodeAudioProcessorFactory factory;
    auto processor = factory.create(AudioModuleRole::Envelope);
    const std::vector<NodeParameter> parameters {
            { EnvelopeMeshState::parameterId(), "Envelope Snapshot", EnvelopeMeshState::defaultSnapshot() }
    };

    AudioProcessContext noteOn;
    noteOn.frameCount = 8;
    noteOn.timing.sampleRate = 16.0;
    noteOn.parameters = parameters;
    noteOn.voice.events.push_back({ NoteLifecycleType::NoteOn, 3, 0 });
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
        context.parameters = {
                { EnvelopeMeshState::parameterId(), "Envelope Snapshot", snapshot },
                { "red", "Red", String(red) },
                { "blue", "Blue", "0" },
                { "logarithmic", "Logarithmic", logarithmic ? "1" : "0" }
        };
        context.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
        auto processor = factory.create(AudioModuleRole::Envelope);
        processor->process(context);
        return output(context).block.samples;
    };

    const auto linear = render(0.f, false);
    const auto morphed = render(1.f, false);
    const auto logarithmic = render(0.f, true);

    REQUIRE(morphed != linear);
    REQUIRE(logarithmic != linear);
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
    start.parameters = {
            { EnvelopeMeshState::parameterId(), "Envelope Snapshot", initialSnapshot }
    };
    start.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    activeProcessor->process(start);

    AudioProcessContext edited;
    edited.frameCount = 4;
    edited.timing.sampleRate = 16.0;
    edited.parameters = {
            { EnvelopeMeshState::parameterId(), "Envelope Snapshot", editedSnapshot }
    };
    activeProcessor->process(edited);

    auto freshProcessor = factory.create(AudioModuleRole::Envelope);
    AudioProcessContext fresh;
    fresh.frameCount = 4;
    fresh.timing.sampleRate = 16.0;
    fresh.parameters = edited.parameters;
    fresh.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
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

TEST_CASE("Utility audio processors reject mismatched matrix dimensions explicitly", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 3;
    context.inputs = {
            gridPayload({ 1.f, 2.f, 3.f, 4.f, 5.f, 6.f }, 2, 3),
            gridPayload({ 10.f, 20.f, 30.f }, 1, 3)
    };
    factory.create(AudioModuleRole::Add)->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { 11.f, 22.f, 33.f });
    REQUIRE_FALSE(output(context).traversalGrid.isValid());
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
    factory.create(AudioModuleRole::Waveshaper)->process(context);

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

TEST_CASE("Waveshaper processor uses persisted FX rasterizer vertices", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext defaultContext;
    defaultContext.frameCount = 3;
    defaultContext.inputs = {
            gridPayload({ -0.5f, 0.f, 0.5f }, 1, 3)
    };
    factory.create(AudioModuleRole::Waveshaper)->process(defaultContext);

    AudioProcessContext shapedContext;
    shapedContext.frameCount = 3;
    shapedContext.inputs = {
            gridPayload({ -0.5f, 0.f, 0.5f }, 1, 3)
    };
    shapedContext.parameters = {
            { "effect.vertices", "Effect Vertices", "0.062500,0.875000,1.000000;0.937500,0.875000,1.000000" }
    };
    factory.create(AudioModuleRole::Waveshaper)->process(shapedContext);

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
    context.parameters = {
            { "pre", "Pre", "2" },
            { "post", "Post", "1" },
            { "aaFactor", "AA Factor", "4" },
            {
                    "effect.vertices",
                    "Effect Vertices",
                    "0.062500,0.950000,1.000000;0.937500,0.050000,1.000000"
            }
    };

    factory.create(AudioModuleRole::Waveshaper)->process(context);

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
    factory.create(AudioModuleRole::Waveshaper)->process(context);

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
    factory.create(AudioModuleRole::ImpulseResponse)->process(context);

    REQUIRE(output(context).traversalGrid.isValid());
    REQUIRE(output(context).traversalGrid.columns == 2);
    REQUIRE(output(context).traversalGrid.rows == 4);
    REQUIRE(output(context).block.samples != std::vector<float> { 1.f, 0.f, 0.f, 0.f });
    REQUIRE(output(context).traversalGrid.values != context.inputs.front().traversalGrid.values);
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
    unity.parameters = {
            { "size", "Size", "0" },
            { "post", "Post", "0.5" },
            { "highPass", "HighPass", "0" },
            {
                    "Effect Vertices",
                    "Effect Vertices",
                    "0.031250,0.750000,1.000000;0.062500,0.750000,1.000000;1.000000,0.750000,1.000000"
            }
    };
    unityProcessor->process(unity);

    AudioProcessContext quiet = unity;
    quiet.parameters[1].value = "0";
    quietProcessor->process(quiet);

    AudioProcessContext filtered = unity;
    filtered.parameters[2].value = "1";
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

    factory.create(AudioModuleRole::ImpulseResponse)->process(context);

    REQUIRE(output(context).block.samples == std::vector<float> { 1.f, -0.5f, 0.25f, 0.f });
}

TEST_CASE("IR processor is split-block equivalent and preserves its tail", "[cycle-v2][runtime][ir]") {
    NodeAudioProcessorFactory factory;
    auto wholeProcessor = factory.create(AudioModuleRole::ImpulseResponse);
    auto splitProcessor = factory.create(AudioModuleRole::ImpulseResponse);
    const std::vector<NodeParameter> parameters {
            { "size", "Size", "0" },
            { "post", "Post", "0.5" },
            { "highPass", "HighPass", "0" },
            {
                    "Effect Vertices",
                    "Effect Vertices",
                    "0.031250,0.750000,1.000000;0.062500,0.750000,1.000000;1.000000,0.750000,1.000000"
            }
    };
    std::vector<float> wholeInput(128, 0.f);
    wholeInput[0] = 1.f;

    AudioProcessContext whole;
    whole.frameCount = wholeInput.size();
    whole.inputs = { payload(wholeInput) };
    whole.parameters = parameters;
    wholeProcessor->process(whole);

    AudioProcessContext firstHalf;
    firstHalf.frameCount = 64;
    firstHalf.inputs = { payload(std::vector<float>(wholeInput.begin(), wholeInput.begin() + 64)) };
    firstHalf.parameters = parameters;
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
    factory.create(AudioModuleRole::Reverb)->process(context);

    REQUIRE(output(context).traversalGrid.isValid());
    REQUIRE(output(context).block.samples != context.inputs.front().block.samples);
    REQUIRE(output(context).traversalGrid.values != context.inputs.front().traversalGrid.values);
    REQUIRE(std::any_of(
            output(context).traversalGrid.values.begin() + 256,
            output(context).traversalGrid.values.end(),
            [](float value) { return std::abs(value) > 0.000001f; }));
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
    withGridProcessor->process(withGridFirst);

    AudioProcessContext blockOnlyFirst;
    blockOnlyFirst.frameCount = 256;
    blockOnlyFirst.parameters = parameters;
    std::vector<float> impulse(256, 0.f);
    impulse.front() = 1.f;
    blockOnlyFirst.inputs = { payload(impulse) };
    blockOnlyProcessor->process(blockOnlyFirst);

    AudioProcessContext withGridSecond;
    withGridSecond.frameCount = 256;
    withGridSecond.parameters = parameters;
    withGridSecond.inputs = { payload(std::vector<float>(256, 0.f)) };
    withGridProcessor->process(withGridSecond);

    AudioProcessContext blockOnlySecond = withGridSecond;
    blockOnlyProcessor->process(blockOnlySecond);

    REQUIRE(output(withGridSecond).block.samples == output(blockOnlySecond).block.samples);
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
    REQUIRE(context.outputs[0].block.samples.size() == 4);
    REQUIRE(context.outputs[0].block.samples[0] > 0.f);
    REQUIRE(context.outputs[0].traversalGrid.isValid());
    REQUIRE(context.outputs[0].traversalGrid.metadata.valueDomain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(context.outputs[0].traversalGrid.metadata.rowAxis == TraversalGridAxis::Frequency);
    REQUIRE(context.outputs[1].domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(context.outputs[1].block.samples.size() == 4);
    REQUIRE(context.outputs[1].traversalGrid.isValid());
    REQUIRE(context.outputs[1].traversalGrid.metadata.valueDomain == PortDomain::SpectralPhaseSignal);
    REQUIRE(context.outputs[1].traversalGrid.metadata.rowAxis == TraversalGridAxis::Frequency);
}

TEST_CASE("FFT and IFFT cycle processors round trip zero-mean cycle buffers", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext fftContext;
    fftContext.frameCount = 4;
    fftContext.inputs = { gridPayload({ -0.5f, -0.25f, 0.25f, 0.5f }, 1, 4) };
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

    REQUIRE(output(ifftContext).traversalGrid.metadata.valueDomain == PortDomain::TimeSignal);
    REQUIRE(output(ifftContext).traversalGrid.metadata.rowAxis == TraversalGridAxis::Time);
    REQUIRE(output(ifftContext).block.samples[0] == Catch::Approx(-0.5f).margin(1.0e-5f));
    REQUIRE(output(ifftContext).block.samples[1] == Catch::Approx(-0.25f).margin(1.0e-5f));
    REQUIRE(output(ifftContext).block.samples[2] == Catch::Approx(0.25f).margin(1.0e-5f));
    REQUIRE(output(ifftContext).block.samples[3] == Catch::Approx(0.5f).margin(1.0e-5f));
}

TEST_CASE("Mesh source processor generates a deterministic operand when unconnected", "[cycle-v2][runtime]") {
    NodeAudioProcessorFactory factory;

    AudioProcessContext context;
    context.frameCount = 3;
    context.outputPorts = {
            { "out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo }
    };
    factory.create(AudioModuleRole::MeshSource)->process(context);

    REQUIRE(output(context).domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(output(context).block.samples.size() == 3);
    REQUIRE(output(context).traversalGrid.isValid());
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
