#include "TrimeshOscillatorCycleRenderer.h"

#include <Audio/CycleDsp/OscillatorLaneRasterizer.h>
#include <Curve/Curve.h>

namespace CycleV2 {

bool TrimeshOscillatorCycleRenderer::prepare(
        std::shared_ptr<const TrimeshConfiguration> configurationToUse,
        int laneCount) {
    if (configurationToUse == nullptr
            || configurationToUse->mesh == nullptr
            || laneCount < 1
            || laneCount > CycleDsp::maximumUnisonOrder) {
        return false;
    }
    if (Curve::table == nullptr) {
        Curve::calcTable();
    }

    configuration = std::move(configurationToUse);
    preparedLaneCount = laneCount;
    auto* mesh = const_cast<Mesh*>(configuration->mesh.get());
    const auto preparation = Rasterization::VoiceRasterizerPreparation::forMesh(*mesh);
    for (int laneIndex = 0; laneIndex < preparedLaneCount; ++laneIndex) {
        auto& lane = lanes[(size_t) laneIndex];
        lane.rasterizer.setGuideCurveProvider(configuration->guideCurveProvider.get());
        lane.rasterizer.setCalcDepthDimensions(false);
        lane.rasterizer.setScalingMode(Rasterization::PointScalingMode::Bipolar);
        lane.rasterizer.prepare(preparation, { &lane.state });
    }
    reset();
    return true;
}

void TrimeshOscillatorCycleRenderer::reset() {
    for (int laneIndex = 0; laneIndex < preparedLaneCount; ++laneIndex) {
        auto& lane = lanes[(size_t) laneIndex];
        lane.state.reset();
        lane.rasterizer.orphanOldVerts();
        lane.primed = false;
    }
}

void TrimeshOscillatorCycleRenderer::renderCycle(
        const ChainedCycleRenderRequest& request,
        Buffer<float> left,
        Buffer<float> right) {
    if (configuration == nullptr
            || request.laneIndex < 0
            || request.laneIndex >= preparedLaneCount
            || left.size() != request.sampleCount
            || right.size() != request.sampleCount) {
        left.zero();
        right.zero();
        return;
    }

    auto& lane = lanes[(size_t) request.laneIndex];
    CycleDsp::ChainedRasterizationRequest rasterRequest {
            const_cast<Mesh*>(configuration->mesh.get()),
            &lane.state,
            configuration->morph,
            request.voice.phaseCycles,
            request.angleDelta,
            request.laneIndex
    };
    if (!lane.primed) {
        CycleDsp::OscillatorLaneRasterizer::prime(lane.rasterizer, rasterRequest);
        lane.primed = true;
    }
    CycleDsp::OscillatorLaneRasterizer::render(lane.rasterizer, rasterRequest, left);
    left.copyTo(right);
}

}
