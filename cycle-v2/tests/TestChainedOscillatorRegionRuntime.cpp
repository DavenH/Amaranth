#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/OscillatorLaneRasterizer.h>

#include "../src/Runtime/ChainedOscillatorRegionRuntime.h"
#include "../src/Runtime/SpectralOscillatorFrameRenderer.h"
#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../src/Nodes/Trimesh/TrimeshOscillatorCycleRenderer.h"

#include <algorithm>
#include <array>

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
    REQUIRE(left[0] == 0.f);
    REQUIRE(right[0] == 0.f);
    REQUIRE(left[1] == Catch::Approx((1.f + 2.f) * scale));
    REQUIRE(right[1] == Catch::Approx((2.f + 3.f) * scale));
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
