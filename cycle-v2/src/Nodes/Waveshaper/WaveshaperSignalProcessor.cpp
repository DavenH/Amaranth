#include "WaveshaperSignalProcessor.h"

#include "../../Graph/NodeDefinition.h"
#include "../Effect2D/FlatCurvePreparation.h"

#include <Util/NumberUtils.h>

namespace CycleV2 {

namespace {

constexpr float kWaveshaperPadding = 0.125f;

float cycle1GainForParameter(float value) {
    return (float) NumberUtils::fromDecibels(45.f * (2.f * value - 1.f));
}

}

std::shared_ptr<const WaveshaperConfiguration> WaveshaperSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters) {
    auto result = std::make_shared<WaveshaperConfiguration>();
    result->enabled = typedParameterBool(parameters, "enabled", true);
    auto preparedTransfer = std::make_shared<WaveshaperTransfer>();
    FlatCurvePreparation curve(
            "CycleV2WaveshaperConfiguration",
            NodeKind::Waveshaper,
            parameters,
            FXRasterizer::Unipolar);
    if (!curve.prepare()) {
        return {};
    }

    preparedTransfer->rasterizeFrom(curve.sampler(), kWaveshaperPadding);

    result->transfer = std::move(preparedTransfer);
    result->preGain = cycle1GainForParameter(parameterFloat(parameters, "pre", 0.5f));
    result->postGain = cycle1GainForParameter(parameterFloat(parameters, "post", 0.5f));
    const int requestedFactor = (int) parameterFloat(parameters, "aaFactor", 1.f);
    result->oversampleFactor = requestedFactor >= 8 ? 8
            : requestedFactor >= 4 ? 4
            : requestedFactor >= 2 ? 2
            : 1;
    return result;
}

void WaveshaperSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    oversampleMemory.resize(spec.maximumFrameCount * 8);
}

void WaveshaperSignalProcessor::adoptConfiguration(const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::Waveshaper) {
        return;
    }

    configuration = std::static_pointer_cast<const WaveshaperConfiguration>(published.value);
    preGain = configuration->preGain;
    postGain = configuration->postGain;
    oversampleFactor = configuration->oversampleFactor;
    oversampler.setOversampleFactor(oversampleFactor);
    adoptedRevision = published.revision;
}

void WaveshaperSignalProcessor::beginBlock(size_t frameCount) {
    useOversampling = oversampleFactor > 1;
    if (!useOversampling) {
        return;
    }

    const size_t requiredSize = frameCount * (size_t) oversampleFactor;
    if (oversampleMemory.size() < requiredSize) {
        oversampleMemory.resize(requiredSize);
    }
    oversampler.setMemoryBuffer({ oversampleMemory.data(), (int) oversampleMemory.size() });
}

void WaveshaperSignalProcessor::beginTraversalGrid(size_t, size_t) {
    useOversampling = false;
}

void WaveshaperSignalProcessor::endTraversalGrid() {
    useOversampling = oversampleFactor > 1;
}

void WaveshaperSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (configuration == nullptr || configuration->transfer == nullptr) {
        return;
    }

    if (useOversampling) {
        oversampler.startOversamplingBlock(buffer);
    }

    configuration->transfer->process(buffer, preGain, postGain);

    if (useOversampling) {
        oversampler.stopOversamplingBlock();
    }
}

}
