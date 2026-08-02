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

void configure(
        Rasterization::VoiceRasterizer& rasterizer,
        const FixedFrameRasterizationRequest& request) {
    rasterizer.setMesh(request.mesh);
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

bool OscillatorLaneRasterizer::renderFixedFrame(
        Rasterization::VoiceRasterizer& rasterizer,
        const FixedFrameRasterizationRequest& request,
        Buffer<float> output) {
    if (request.mesh == nullptr || output.empty()) {
        output.zero();
        return false;
    }

    configure(rasterizer, request);
    const double interval = 1.0 / (double) output.size();
    rasterizer.setInterceptPadding((float) interval * 2.f);
    rasterizer.renderOrdinary(request.mesh, request.phaseCycles);
    const auto sampler = rasterizer.sampler();
    if (!sampler.isSampleable()) {
        output.zero();
        return false;
    }

    if (rasterizer.doesIntegralSampling()) {
        sampler.samplePerfectly(interval, output, 0.0);
    } else {
        sampler.sampleWithInterval(output, interval, 0.0);
    }
    return true;
}

}
