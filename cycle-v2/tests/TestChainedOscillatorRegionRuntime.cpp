#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/OscillatorLaneRasterizer.h>
#include <Util/LogRegionMapping.h>

#include "Runtime/ChainedOscillatorRegionRuntime.h"
#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/PreparedOscillatorRegion.h"
#include "Runtime/SpectralOscillatorFrameRenderer.h"
#include "Runtime/SpectralOscillatorRegionRuntime.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Dsp/TrimeshOscillatorCycleRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace CycleV2;

namespace {

class ConstantCycleRenderer final : public OscillatorCycleRenderer {
public:
    void renderCycle(
            const ChainedCycleRenderRequest& request,
            Buffer<float> left,
            Buffer<float> right) override {
        left.set((float) request.laneIndex + 1.f);
        right.set((float) request.laneIndex + 1.f);
        ++renderCounts[(size_t) request.laneIndex];
        renderedPhases[(size_t) request.laneIndex] = request.voice.phaseCycles;
    }

    std::array<int, CycleDsp::maximumUnisonOrder> renderCounts {};
    std::array<float, CycleDsp::maximumUnisonOrder> renderedPhases {};
};

struct PartitionedRender {
    std::vector<float> left;
    std::vector<float> right;
    std::vector<float> scratch;
    size_t frameRenderCount {};
};

class ScratchCaptureObserver final : public GraphProcessObserver {
public:
    explicit ScratchCaptureObserver(PartitionedRender& render) : render(render) {}

    void nodeProcessed(
            const String& nodeId,
            const AudioProcessContext& context) override {
        if (nodeId != "scratchEnvelope1" || context.outputs.empty()) {
            return;
        }
        const auto& block = context.outputs.front().block.samples;
        render.scratch.insert(render.scratch.end(), block.begin(), block.end());
    }

private:
    PartitionedRender& render;
};

float maximumDifference(
        const std::vector<float>& left,
        const std::vector<float>& right) {
    REQUIRE(left.size() == right.size());
    float result = 0.f;
    for (size_t index = 0; index < left.size(); ++index) {
        result = std::max(result, std::abs(left[index] - right[index]));
    }
    return result;
}

NodeGraph loadPresetGraph(const String& name) {
#if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile(name + ".cyclegraph");
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    return loaded.graph;
#else
    return {};
#endif
}

NodeGraph loadOscillatorPresetGraph(const String& name) {
    NodeGraph graph = loadPresetGraph(name);
    for (const auto& node : graph.getNodes()) {
        const bool downstreamEffect = node.kind == NodeKind::ImpulseResponse
                || node.kind == NodeKind::Waveshaper
                || node.kind == NodeKind::Reverb
                || node.kind == NodeKind::Delay
                || node.kind == NodeKind::Equalizer;
        if (!downstreamEffect) {
            continue;
        }
        auto parameters = node.parameters;
        for (auto& parameter : parameters) {
            if (parameter.id == "enabled") {
                parameter.value = "0";
            }
        }
        REQUIRE(graph.replaceNodeParameters(node.id, std::move(parameters)));
    }
    return graph;
}

NodeGraph loadFilterSawGraph() {
    return loadPresetGraph("filter-saw");
}

GraphExecutionPlan loadFilterSawPlan() {
#if defined(CYCLE_V2_SOURCE_DIR)
    const auto compiled = GraphCompiler().compile(loadFilterSawGraph());
    REQUIRE(compiled.succeeded());
    return compiled.plan;
#else
    return {};
#endif
}

TEST_CASE("Prepared trimesh morph binding preserves fractional voice position",
        "[cycle-v2][runtime][oscillator-region][voice-time][parity]") {
#if defined(CYCLE_V2_SOURCE_DIR)
    const auto plan = loadFilterSawPlan();
    const auto timeLayer = std::find_if(
            plan.steps.begin(),
            plan.steps.end(),
            [](const GraphExecutionStep& step) {
                return step.nodeId == "timeLayer1";
            });
    REQUIRE(timeLayer != plan.steps.end());

    PreparedTrimeshMorphBinding binding;
    binding.bind(plan, *timeLayer);
    AudioVoiceContext voice;
    voice.controls.normalizedVoiceTimeIncrement = 0.001f;
    PreparedOscillatorProcessContext context;
    context.voice = &voice;

    const auto inputs = binding.inputsFor(context, 0, 337.75);

    REQUIRE(inputs.hasAbsoluteOverride[0]);
    REQUIRE(inputs.absoluteOverrides[0] == Catch::Approx(0.33775f));
    REQUIRE(inputs.absoluteOverrides[0] != Catch::Approx(0.337f));
#else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
#endif
}

PartitionedRender renderPreparedGraph(
        const GraphExecutionPlan& plan,
        int blockSize,
        int sampleCount,
        int controllerEventOffset = -1,
        int midiNote = 72,
        float voiceDurationSeconds = 1.f) {
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 512;
    spec.sampleRate = 48000.0;
    GraphAudioExecutor executor;
    executor.prepareExecution(plan, spec);

    PartitionedRender result;
    result.left.reserve((size_t) sampleCount);
    result.right.reserve((size_t) sampleCount);
    AudioVoiceContext voice;
    voice.hasLifecycleSeed = true;
    voice.lifecycleSeed = 0x43594332u;
    voice.controls.noteNumber = midiNote;
    voice.controls.velocity = 1.f;
    voice.controls.normalizedVoiceTimeIncrement
            = 1.f / (48000.f * voiceDurationSeconds);
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    ScratchCaptureObserver observer(result);
    if (controllerEventOffset >= 0) {
        voice.controlEvents.push_back({
                ControlEventKind::Controller,
                (size_t) controllerEventOffset,
                74,
                1.f
        });
    }
    for (int start = 0; start < sampleCount; start += blockSize) {
        const int count = std::min(blockSize, sampleCount - start);
        voice.controls.normalizedVoiceTime
                = (float) start / (48000.f * voiceDurationSeconds);
        const auto output = executor.processRealtime(
                plan,
                (size_t) count,
                { 48000.0, 120.0, 4 },
                voice,
                &observer);
        REQUIRE(output.isValid());
        result.left.insert(
                result.left.end(),
                output.payload->block.samples.begin(),
                output.payload->block.samples.end());
        const SignalBlock& right = output.payload->isStereo()
                ? output.payload->secondaryBlock
                : output.payload->block;
        result.right.insert(
                result.right.end(),
                right.samples.begin(),
                right.samples.end());
        voice.events.clear();
        voice.controlEvents.clear();
    }
    result.frameRenderCount = executor.oscillatorFrameRenderCount(0);
    return result;
}

std::array<std::vector<float>, 2> renderPreparedPresetFrames(
        const String& presetName) {
    const auto compiled = GraphCompiler().compile(loadPresetGraph(presetName));
    REQUIRE(compiled.succeeded());
    const auto region = std::find_if(
            compiled.plan.oscillatorRegions.begin(),
            compiled.plan.oscillatorRegions.end(),
            [](const OscillatorRegionPlan& candidate) {
                return candidate.strategy
                        == OscillatorExecutionStrategy::SharedSpectralFrame;
            });
    REQUIRE(region != compiled.plan.oscillatorRegions.end());

    SpectralOscillatorFrameRenderer renderer;
    REQUIRE(renderer.prepare(compiled.plan, *region, 4096));
    std::vector<SignalPayload> signals(compiled.plan.buffers.size());
    for (auto& signal : signals) {
        signal.domain = PortDomain::ControlSignal;
        signal.channelLayout = ChannelLayout::Mono;
        signal.block.samples.assign(512, 0.05f);
    }
    AudioVoiceContext voice;
    voice.hasLifecycleSeed = true;
    voice.lifecycleSeed = 0x43594332u;
    PreparedOscillatorProcessContext context;
    context.voice = &voice;
    context.signalBuffers = signals.data();
    context.signalBufferCount = signals.size();
    context.blockFrameCount = 512;
    context.timing.sampleRate = 48000.0;

    std::array<std::vector<float>, 2> result;
    std::array<float, 512> left {};
    std::array<float, 512> right {};
    REQUIRE(renderer.renderFrame(
            512,
            60,
            context,
            0,
            0,
            1,
            Buffer<float>(left.data(), (int) left.size()),
            Buffer<float>(right.data(), (int) right.size())));
    result[0] = std::vector<float>(left.begin(), left.end());

    for (auto& signal : signals) {
        signal.block.samples.assign(512, 0.37f);
    }
    REQUIRE(renderer.renderFrame(
            512,
            60,
            context,
            511,
            511,
            4096,
            Buffer<float>(left.data(), (int) left.size()),
            Buffer<float>(right.data(), (int) right.size())));
    result[1] = std::vector<float>(left.begin(), left.end());
    return result;
}

bool presetHasEvolvingSpectralRaster(const String& presetName) {
    const auto compiled = GraphCompiler().compile(loadPresetGraph(presetName));
    REQUIRE(compiled.succeeded());
    for (const auto& step : compiled.plan.steps) {
        if (step.audioRole != AudioModuleRole::MeshSource
                || step.outputs.empty()
                || (step.outputs.front().domain
                        != PortDomain::SpectralMagnitudeSignal
                    && step.outputs.front().domain
                        != PortDomain::SpectralPhaseSignal)) {
            continue;
        }
        const auto configuration = std::dynamic_pointer_cast<
                const TrimeshConfiguration>(step.configuration.value);
        REQUIRE(configuration != nullptr);
        TrimeshBlockwiseDsp dsp;
        dsp.setGuideCurveProvider(configuration->guideCurveProvider.get());
        dsp.prepare(
                const_cast<Mesh*>(configuration->mesh.get()),
                MorphPosition(0.05f, 0.05f, 0.05f),
                configuration->primaryViewAxis,
                false,
                step.outputs.front().domain);
        dsp.prepareSampling(256);
        dsp.setFrequencyMidiNote(60 + LogRegionMapping::legacyMidiNoteBias);
        std::array<float, 256> early {};
        std::array<float, 256> late {};
        dsp.rasterizePrepared(17);
        dsp.renderPreparedHarmonicsInto(
                Buffer<float>(early.data(), (int) early.size()));
        dsp.setMorphPosition(MorphPosition(0.37f, 0.37f, 0.37f));
        dsp.rasterizePrepared(19);
        dsp.renderPreparedHarmonicsInto(
                Buffer<float>(late.data(), (int) late.size()));
        if (early != late) {
            return true;
        }
    }
    return false;
}

}

TEST_CASE("Chained oscillator runtime folds prepared lanes with Cycle 1 pan and level",
        "[cycle-v2][runtime][oscillator-region][unison]") {
    CycleDsp::UnisonIndividualConfiguration configuration;
    configuration.order = 3;
    configuration.detunePositions[0] = 0.25f;
    configuration.detunePositions[1] = 0.5f;
    configuration.detunePositions[2] = 0.75f;
    configuration.pans[0] = 0.f;
    configuration.pans[1] = 0.5f;
    configuration.pans[2] = 1.f;
    configuration.phaseCycles[0] = 0.1f;
    configuration.phaseCycles[1] = 0.2f;
    configuration.phaseCycles[2] = 0.3f;
    const auto layout = CycleDsp::UnisonCore::makeIndividualLayout(configuration);
    ChainedOscillatorRegionRuntime runtime;
    REQUIRE(runtime.prepare(64, 2048, 44100.0, layout));

    std::array<float, 64> left {};
    std::array<float, 64> right {};
    ConstantCycleRenderer renderer;
    REQUIRE(runtime.process(
            60,
            1.f,
            {},
            Buffer<float>(left.data(), (int) left.size()),
            Buffer<float>(right.data(), (int) right.size()),
            renderer));

    const float scale = CycleDsp::UnisonCore::voiceLevelScale(3);
    REQUIRE(left[0] == Catch::Approx((1.f + 2.f) * scale));
    REQUIRE(right[0] == Catch::Approx((2.f + 3.f) * scale));
    REQUIRE(left[1] == Catch::Approx(left[0]));
    REQUIRE(right[1] == Catch::Approx(right[0]));
    REQUIRE(renderer.renderCounts[0] > 0);
    REQUIRE(renderer.renderCounts[1] > 0);
    REQUIRE(renderer.renderCounts[2] > 0);
    REQUIRE(renderer.renderedPhases[0] == Catch::Approx(0.1f));
    REQUIRE(renderer.renderedPhases[2] == Catch::Approx(0.3f));
}

TEST_CASE("Chained oscillator runtime preserves continuity across split blocks",
        "[cycle-v2][runtime][oscillator-region][unison][split-block]") {
    CycleDsp::UnisonGroupConfiguration configuration;
    configuration.order = 1;
    const auto layout = CycleDsp::UnisonCore::makeGroupLayout(configuration);
    ChainedOscillatorRegionRuntime contiguous;
    ChainedOscillatorRegionRuntime split;
    REQUIRE(contiguous.prepare(64, 2048, 44100.0, layout));
    REQUIRE(split.prepare(64, 2048, 44100.0, layout));

    std::array<float, 64> wholeLeft {};
    std::array<float, 64> wholeRight {};
    std::array<float, 64> splitLeft {};
    std::array<float, 64> splitRight {};
    ConstantCycleRenderer wholeRenderer;
    ConstantCycleRenderer splitRenderer;
    REQUIRE(contiguous.process(
            60, 1.f, {},
            Buffer<float>(wholeLeft.data(), 64),
            Buffer<float>(wholeRight.data(), 64),
            wholeRenderer));
    REQUIRE(split.process(
            60, 1.f, {},
            Buffer<float>(splitLeft.data(), 23),
            Buffer<float>(splitRight.data(), 23),
            splitRenderer));
    REQUIRE(split.process(
            60, 1.f, {},
            Buffer<float>(splitLeft.data() + 23, 41),
            Buffer<float>(splitRight.data() + 23, 41),
            splitRenderer));

    REQUIRE(splitLeft == wholeLeft);
    REQUIRE(splitRight == wholeRight);
    REQUIRE(splitRenderer.renderCounts == wholeRenderer.renderCounts);
}

TEST_CASE("Trimesh oscillator lanes consume the mature chained VoiceRasterizer",
        "[cycle-v2][runtime][oscillator-region][unison][trimesh]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh("ChainedRegionTrimesh");
    auto configuration = std::make_shared<TrimeshConfiguration>();
    configuration->mesh = std::shared_ptr<const Mesh>(mesh.get(), [](const Mesh*) {});
    configuration->morph = MorphPosition(0.5f, 0.5f, 0.5f);
    CycleDsp::UnisonGroupConfiguration unison;
    unison.order = 3;
    unison.detuneWidthCents = 12.f;
    const auto layout = CycleDsp::UnisonCore::makeGroupLayout(unison);
    TrimeshOscillatorCycleRenderer renderer;
    ChainedOscillatorRegionRuntime runtime;
    REQUIRE(renderer.prepare(configuration, layout.order));
    REQUIRE(runtime.prepare(256, 4096, 44100.0, layout));

    std::array<float, 256> left {};
    std::array<float, 256> right {};
    REQUIRE(runtime.process(
            60, 1.f, {},
            Buffer<float>(left.data(), (int) left.size()),
            Buffer<float>(right.data(), (int) right.size()),
            renderer));

    REQUIRE(std::any_of(left.begin(), left.end(), [](float sample) {
        return sample != 0.f;
    }));
    REQUIRE(std::any_of(right.begin(), right.end(), [](float sample) {
        return sample != 0.f;
    }));
    REQUIRE(left != right);

    configuration.reset();
    mesh->destroy();
}

TEST_CASE("Spectral oscillator recipes preserve a fixed Trimesh frame through FFT",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][trimesh]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", {}));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", {}));
    REQUIRE(GraphEditor().connect(
            graph,
            { "voice", "context", false },
            { "mesh", "context", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "mesh", "out", false },
            { "fft", "time", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "fft", "mag", false },
            { "ifft", "mag", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "fft", "phase", false },
            { "ifft", "phase", true }).succeeded());
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    REQUIRE(compiled.plan.oscillatorRegions.size() == 1);
    const auto& region = compiled.plan.oscillatorRegions.front();
    REQUIRE(region.strategy == OscillatorExecutionStrategy::SharedSpectralFrame);

    SpectralOscillatorFrameRenderer renderer;
    REQUIRE(renderer.prepare(compiled.plan, region, 16384));
    constexpr int frameSize = 256;
    std::array<float, frameSize> left {};
    std::array<float, frameSize> right {};
    REQUIRE(renderer.renderFrame(
            frameSize,
            60,
            Buffer<float>(left.data(), frameSize),
            Buffer<float>(right.data(), frameSize)));
    REQUIRE(left == right);

    const auto& meshStep = *std::find_if(
            compiled.plan.steps.begin(),
            compiled.plan.steps.end(),
            [](const GraphExecutionStep& step) {
                return step.nodeId == "mesh";
            });
    const auto configuration = std::dynamic_pointer_cast<const TrimeshConfiguration>(
            meshStep.configuration.value);
    REQUIRE(configuration != nullptr);
    Rasterization::VoiceCycleState expectedState;
    Rasterization::VoiceRasterizer expectedRasterizer;
    expectedRasterizer.setCalcDepthDimensions(false);
    expectedRasterizer.setScalingMode(Rasterization::PointScalingMode::Bipolar);
    expectedRasterizer.prepare(
            Rasterization::VoiceRasterizerPreparation::forMesh(
                    *const_cast<Mesh*>(configuration->mesh.get())),
            { &expectedState });
    std::array<float, frameSize> expected {};
    REQUIRE(CycleDsp::OscillatorLaneRasterizer::renderFixedFrame(
            expectedRasterizer,
            {
                    const_cast<Mesh*>(configuration->mesh.get()),
                    configuration->morph,
                    0.f,
                    0
            },
            Buffer<float>(expected.data(), frameSize)));
    REQUIRE(Buffer<float>(left.data(), frameSize).normDiffL2({
                    expected.data(),
                    frameSize
            }) < 1.0e-5f);
}

TEST_CASE("Stengah phase layer pans survive spectral materialization",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][pan][preset]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("stengah.cyclegraph");
    REQUIRE(preset.existsAsFile());
    NodeGraph graph = GraphSerializer().fromJsonString(preset.loadFileAsString());

    const auto render = [](const NodeGraph& graphToRender) {
        const auto compiled = GraphCompiler().compile(graphToRender);
        REQUIRE(compiled.succeeded());
        REQUIRE(compiled.plan.oscillatorRegions.size() == 1);

        SpectralOscillatorFrameRenderer renderer;
        REQUIRE(renderer.prepare(
                compiled.plan,
                compiled.plan.oscillatorRegions.front(),
                16384));
        constexpr int frameSize = 256;
        std::array<float, frameSize> left {};
        std::array<float, frameSize> right {};
        REQUIRE(renderer.renderFrame(
                frameSize,
                60,
                Buffer<float>(left.data(), frameSize),
                Buffer<float>(right.data(), frameSize)));
        return std::pair { left, right };
    };

    auto authored = render(graph);
    REQUIRE(Buffer<float>(
            authored.first.data(),
            (int) authored.first.size()).normDiffL2({
                    authored.second.data(),
                    (int) authored.second.size()
            }) > 0.01f);

    REQUIRE(GraphEditor().setNodeParameter(
            graph, "phaseLayer1Process", "pan", "Layer Pan", "0.0").succeeded());
    REQUIRE(GraphEditor().setNodeParameter(
            graph, "phaseLayer2Process", "pan", "Layer Pan", "1.0").succeeded());
    auto swapped = render(graph);

    REQUIRE(Buffer<float>(
            authored.first.data(),
            (int) authored.first.size()).normDiffL2({
                    swapped.second.data(),
                    (int) swapped.second.size()
            }) < 1.0e-5f);
    REQUIRE(Buffer<float>(
            authored.second.data(),
            (int) authored.second.size()).normDiffL2({
                    swapped.first.data(),
                    (int) swapped.first.size()
            }) < 1.0e-5f);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Spectral oscillator runtime reconstructs one shared frame across Unison lanes",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][unison][split-block]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", {}));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", {}));
    REQUIRE(GraphEditor().connect(
            graph,
            { "voice", "context", false },
            { "mesh", "context", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "mesh", "out", false },
            { "fft", "time", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "fft", "mag", false },
            { "ifft", "mag", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "fft", "phase", false },
            { "ifft", "phase", true }).succeeded());
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    const auto& region = compiled.plan.oscillatorRegions.front();

    CycleDsp::UnisonIndividualConfiguration unison;
    unison.order = 3;
    unison.detunePositions[0] = 0.25f;
    unison.detunePositions[1] = 0.5f;
    unison.detunePositions[2] = 0.75f;
    unison.pans[0] = 0.f;
    unison.pans[1] = 0.5f;
    unison.pans[2] = 1.f;
    unison.phaseCycles[0] = 0.1f;
    unison.phaseCycles[1] = 0.2f;
    unison.phaseCycles[2] = 0.3f;
    const auto layout = CycleDsp::UnisonCore::makeIndividualLayout(unison);

    SpectralOscillatorFrameRenderer wholeRenderer;
    SpectralOscillatorFrameRenderer splitRenderer;
    REQUIRE(wholeRenderer.prepare(compiled.plan, region, 16384));
    REQUIRE(splitRenderer.prepare(compiled.plan, region, 16384));
    SpectralOscillatorRegionRuntime wholeRuntime;
    SpectralOscillatorRegionRuntime splitRuntime;
    REQUIRE(wholeRuntime.prepare(128, 4096, 16384, 44100.0, layout));
    REQUIRE(splitRuntime.prepare(128, 4096, 16384, 44100.0, layout));

    std::array<float, 128> wholeLeft {};
    std::array<float, 128> wholeRight {};
    std::array<float, 128> splitLeft {};
    std::array<float, 128> splitRight {};
    REQUIRE(wholeRuntime.process(
            60, 1.f, {},
            Buffer<float>(wholeLeft.data(), 128),
            Buffer<float>(wholeRight.data(), 128),
            wholeRenderer));
    REQUIRE(splitRuntime.process(
            60, 1.f, {},
            Buffer<float>(splitLeft.data(), 37),
            Buffer<float>(splitRight.data(), 37),
            splitRenderer));
    REQUIRE(splitRuntime.process(
            60, 1.f, {},
            Buffer<float>(splitLeft.data() + 37, 91),
            Buffer<float>(splitRight.data() + 37, 91),
            splitRenderer));

    REQUIRE(wholeRenderer.frameRenderCount() == 2);
    REQUIRE(splitRenderer.frameRenderCount() == 2);
    REQUIRE(splitLeft == wholeLeft);
    REQUIRE(splitRight == wholeRight);
    REQUIRE(std::any_of(wholeLeft.begin(), wholeLeft.end(), [](float sample) {
        return sample != 0.f;
    }));
    REQUIRE(std::any_of(wholeRight.begin(), wholeRight.end(), [](float sample) {
        return sample != 0.f;
    }));
    REQUIRE(wholeLeft != wholeRight);
}

TEST_CASE("Evolving spectral frames are independent of host block partitions",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][live-modulation]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const GraphExecutionPlan plan = loadFilterSawPlan();
    const PartitionedRender reference = renderPreparedGraph(plan, 512, 2048);
    REQUIRE(reference.frameRenderCount > 1);

    for (const int blockSize : { 64, 127, 256 }) {
        DYNAMIC_SECTION("block size " << blockSize) {
            const PartitionedRender partitioned = renderPreparedGraph(
                    plan,
                    blockSize,
                    2048);
            REQUIRE(partitioned.frameRenderCount == reference.frameRenderCount);
            REQUIRE(partitioned.left == reference.left);
            REQUIRE(partitioned.right == reference.right);
        }
    }
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("High spectral notes use the legacy 16-sample control cadence",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][control-rate]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const PartitionedRender render = renderPreparedGraph(
            loadFilterSawPlan(),
            512,
            128,
            -1,
            120);

    REQUIRE(render.frameRenderCount == 9);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Prepared oscillator preset matrix is independent of host block partitions",
        "[cycle-v2][runtime][oscillator-region][live-modulation][partition-matrix]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    struct PresetCase {
        String name;
        float voiceDurationSeconds;
    };
    const std::array presets {
            PresetCase { "saw", 1.2669865f },
            PresetCase { "filter-saw", 0.47689545f },
            PresetCase { "pwm", 1.2669865f },
            PresetCase { "dunk-2", 0.5069265f },
            PresetCase { "japan-drum", 1.0113605f }
    };
    for (const auto& preset : presets) {
        const auto compiled = GraphCompiler().compile(
                loadOscillatorPresetGraph(preset.name));
        REQUIRE(compiled.succeeded());
        REQUIRE_FALSE(compiled.plan.oscillatorRegions.empty());
        REQUIRE(std::all_of(
                compiled.plan.oscillatorRegions.begin(),
                compiled.plan.oscillatorRegions.end(),
                [&](const OscillatorRegionPlan& region) {
                    return supportsPreparedOscillatorRegion(compiled.plan, region);
                }));
        for (const int midiNote : { 36, 48, 60, 72 }) {
            for (const int sampleCount : { 1024, 4096 }) {
                DYNAMIC_SECTION(preset.name << " note " << midiNote
                        << " samples " << sampleCount) {
                    const PartitionedRender reference = renderPreparedGraph(
                            compiled.plan,
                            512,
                            sampleCount,
                            -1,
                            midiNote,
                            preset.voiceDurationSeconds);
                    for (const int blockSize : { 64, 127, 256 }) {
                        const PartitionedRender partitioned = renderPreparedGraph(
                                compiled.plan,
                                blockSize,
                                sampleCount,
                                -1,
                                midiNote,
                                preset.voiceDurationSeconds);
                        REQUIRE(partitioned.frameRenderCount
                                == reference.frameRenderCount);
                        REQUIRE(maximumDifference(
                                partitioned.scratch,
                                reference.scratch) == 0.f);
                        REQUIRE(maximumDifference(
                                partitioned.left,
                                reference.left) == 0.f);
                        REQUIRE(maximumDifference(
                                partitioned.right,
                                reference.right) == 0.f);
                    }
                }
            }
        }
    }
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Prepared spectral preset frames rerasterize at live morph positions",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][live-modulation]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const auto filterSawFrames = renderPreparedPresetFrames("filter-saw");
    REQUIRE(filterSawFrames[0] != filterSawFrames[1]);
    REQUIRE(presetHasEvolvingSpectralRaster("japan-drum"));
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Timed controls enter prepared frames at the synthesis-cycle frontier",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][timed-control]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    NodeGraph graph = loadFilterSawGraph();
    REQUIRE(GraphEditor().setNodeParameter(
            graph,
            "morph",
            "yellowSource",
            "Yellow Source",
            "midiCC").succeeded());
    REQUIRE(GraphEditor().setNodeParameter(
            graph,
            "morph",
            "yellowController",
            "Yellow Controller",
            "74").succeeded());
    REQUIRE(GraphEditor().setNodeParameter(
            graph,
            "scratchEnvelope1",
            "enabled",
            "Enabled",
            "0").succeeded());
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());

    const PartitionedRender before = renderPreparedGraph(compiled.plan, 512, 512, 90);
    const PartitionedRender on = renderPreparedGraph(compiled.plan, 512, 512, 91);
    const PartitionedRender after = renderPreparedGraph(compiled.plan, 512, 512, 92);
    REQUIRE(before.left == on.left);
    REQUIRE(before.right == on.right);
    REQUIRE(after.left != on.left);
    REQUIRE(after.right != on.right);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Prepared spectral scratch follows the authored envelope attachment",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][scratch]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    NodeGraph enabledGraph = loadFilterSawGraph();
    const auto enabled = GraphCompiler().compile(enabledGraph);
    REQUIRE(enabled.succeeded());

    NodeGraph disabledGraph = enabledGraph;
    REQUIRE(GraphEditor().setNodeParameter(
            disabledGraph,
            "scratchEnvelope1",
            "enabled",
            "Enabled",
            "0").succeeded());
    const auto disabled = GraphCompiler().compile(disabledGraph);
    REQUIRE(disabled.succeeded());

    const PartitionedRender withScratch = renderPreparedGraph(
            enabled.plan, 256, 2048);
    const PartitionedRender withoutScratch = renderPreparedGraph(
            disabled.plan, 256, 2048);
    REQUIRE(withScratch.left != withoutScratch.left);
    REQUIRE(withScratch.right != withoutScratch.right);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Spectral frame refresh count is independent of Unison order",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][unison][live-modulation]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    NodeGraph graph = loadFilterSawGraph();
    REQUIRE(GraphEditor().setNodeParameter(
            graph,
            "scratchEnvelope1",
            "enabled",
            "Enabled",
            "0").succeeded());
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    GraphExecutionPlan singlePlan = compiled.plan;
    GraphExecutionPlan unisonPlan = singlePlan;
    REQUIRE(singlePlan.voiceContexts.size() == 1);
    REQUIRE(unisonPlan.voiceContexts.size() == 1);

    CycleDsp::UnisonGroupConfiguration singleConfiguration;
    singleConfiguration.order = 1;
    singlePlan.voiceContexts.front().lanes
            = CycleDsp::UnisonCore::makeGroupLayout(singleConfiguration);

    CycleDsp::UnisonGroupConfiguration unisonConfiguration;
    unisonConfiguration.order = 4;
    unisonConfiguration.detuneWidthCents = 18.f;
    unisonConfiguration.panSpread = 1.f;
    unisonPlan.voiceContexts.front().lanes
            = CycleDsp::UnisonCore::makeGroupLayout(unisonConfiguration);

    const PartitionedRender single = renderPreparedGraph(singlePlan, 256, 2048);
    const PartitionedRender unison = renderPreparedGraph(unisonPlan, 256, 2048);
    REQUIRE(single.frameRenderCount > 1);
    REQUIRE(unison.frameRenderCount == single.frameRenderCount);
    REQUIRE(unison.left != unison.right);
    for (const int blockSize : { 64, 127, 512 }) {
        DYNAMIC_SECTION("Unison block size " << blockSize) {
            const PartitionedRender partitioned = renderPreparedGraph(
                    unisonPlan,
                    blockSize,
                    2048);
            REQUIRE(partitioned.frameRenderCount == unison.frameRenderCount);
            REQUIRE(maximumDifference(partitioned.left, unison.left) < 1.0e-3f);
            REQUIRE(maximumDifference(partitioned.right, unison.right) < 1.0e-3f);
        }
    }
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Prepared spectral regions reject unresolved live controls",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][fallback]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    GraphExecutionPlan plan = loadFilterSawPlan();
    REQUIRE(plan.oscillatorRegions.size() == 1);
    REQUIRE(SpectralOscillatorFrameRenderer::supports(
            plan,
            plan.oscillatorRegions.front()));
    bool invalidated = false;
    for (auto& step : plan.steps) {
        for (auto& input : step.inputs) {
            if (input.destPortId == "yellow") {
                input.sourceBufferIndex = -1;
                invalidated = true;
                break;
            }
        }
        if (invalidated) {
            break;
        }
    }
    REQUIRE(invalidated);
    REQUIRE_FALSE(SpectralOscillatorFrameRenderer::supports(
            plan,
            plan.oscillatorRegions.front()));
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
