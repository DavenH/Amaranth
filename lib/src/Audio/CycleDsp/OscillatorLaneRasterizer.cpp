#include "OscillatorLaneRasterizer.h"

#include <algorithm>

namespace CycleDsp {

namespace {

void configure(
        Rasterization::VoiceRasterizer& rasterizer,
        const ChainedRasterizationRequest& request) {
    rasterizer.setMesh(request.mesh);
    rasterizer.setState(request.state);
    rasterizer.setMorphPosition(request.morph);
    rasterizer.setNoiseSeed(request.noiseSeed);
    rasterizer.setWrapsEnds(true);
}

}

void OscillatorLaneRasterizer::prime(
        Rasterization::VoiceRasterizer& rasterizer,
        const ChainedRasterizationRequest& request) {
    if (request.state == nullptr) {
        return;
    }
    configure(rasterizer, request);
    rasterizer.setInterceptPadding((float) request.angleDelta);
    rasterizer.renderChained(request.phaseCycles);
}

void OscillatorLaneRasterizer::render(
        Rasterization::VoiceRasterizer& rasterizer,
        const ChainedRasterizationRequest& request,
        Buffer<float> output) {
    if (request.state == nullptr || request.angleDelta <= 0.0) {
        output.zero();
        return;
    }

    configure(rasterizer, request);
    rasterizer.setInterceptPadding((float) std::max(
            -request.state->spillover,
            request.angleDelta));
    rasterizer.renderChained(request.phaseCycles);

    const auto sampler = rasterizer.sampler();
    if (sampler.isSampleable()) {
        request.state->spillover = sampler.sampleWithInterval(
                output,
                request.angleDelta,
                request.state->spillover);
        return;
    }

    output.zero();
    request.state->spillover += output.size() * request.angleDelta;
    if (request.state->spillover > 0.5) {
        request.state->spillover -= 1.0;
    }
}

}
