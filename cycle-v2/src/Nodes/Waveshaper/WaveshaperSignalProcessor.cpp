#include "Nodes/Waveshaper/WaveshaperSignalProcessor.h"

#include "Graph/NodeParameterMap.h"
#include "Graph/NodeDefinition.h"
#include "Nodes/Curve/Panel/FlatCurvePreparation.h"

#include <Audio/CycleDsp/EffectParameterMapping.h>
#include <Util/NumberUtils.h>

namespace CycleV2 {

namespace {

constexpr float kWaveshaperPadding = 0.125f;

float gainForParameter(float value) {
    return (float) NumberUtils::fromDecibels(CycleDsp::waveshaperGainDecibels(value));
}

}

std::shared_ptr<const WaveshaperConfiguration> WaveshaperSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model) {
    auto result = std::make_shared<WaveshaperConfiguration>();
    const NodeParameterMap parameterMap(parameters);
    result->enabled = parameterMap.boolValue("enabled", true);
    auto preparedTransfer = std::make_shared<WaveshaperTransfer>();
    FlatCurvePreparation curve(
            "CycleV2WaveshaperConfiguration",
            model,
            FXRasterizer::Unipolar);
    if (!curve.prepare()) {
        return {};
    }

    preparedTransfer->rasterizeFrom(curve.sampler(), kWaveshaperPadding);

    result->transfer = std::move(preparedTransfer);
    result->preGain = gainForParameter(parameterMap.floatValue("pre", 0.5f));
    result->postGain = gainForParameter(parameterMap.floatValue("post", 0.5f));
    const int requestedFactor = parameterMap.intValue("aaFactor", 1);
    result->oversampleFactor = requestedFactor >= 8 ? 8
            : requestedFactor >= 4 ? 4
            : requestedFactor >= 2 ? 2
            : 1;
    return result;
}

void WaveshaperSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    oversampleMemory.resize((int) (spec.maximumFrameCount * 8));
    oversampler.setMemoryBuffer(oversampleMemory);
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
    if ((size_t) oversampleMemory.size() < requiredSize) {
        jassertfalse;
        useOversampling = false;
        return;
    }
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
