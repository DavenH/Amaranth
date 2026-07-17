#include "EffectSignalProcessors.h"

#include "../../Graph/NodeDefinition.h"
#include "../Effect2D/FlatCurvePreparation.h"

#include <Algo/ConvReverb.h>
#include <Algo/FFT.h>
#include <Array/ScopedAlloc.h>
#include <Audio/CycleDsp/ReverbKernel.h>
#include <Util/NumberUtils.h>

#include <algorithm>
#include <cmath>

namespace CycleV2 {

namespace {

constexpr float kIrPadding = 0.0625f;

}

std::shared_ptr<const IrConfiguration> IrSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters) {
    auto result = std::make_shared<IrConfiguration>();
    result->enabled = typedParameterBool(parameters, "enabled", true);
    const float highPass = parameterFloat(parameters, "highPass", 0.5f);
    const size_t impulseLength = (size_t) CycleDsp::irImpulseLength(
            parameterFloat(parameters, "size", 0.5f));
    FlatCurvePreparation curve(
            "CycleV2IrConfiguration",
            NodeKind::ImpulseResponse,
            parameters,
            FXRasterizer::Bipolar);
    if (!curve.prepare()) {
        return {};
    }

    result->impulse.resize(impulseLength);
    std::vector<float> rawImpulse(impulseLength);
    std::vector<float> oversampledImpulse(impulseLength * 2);
    std::vector<float> prefilterLevels(impulseLength / 2);
    Oversampler preparedOversampler(8);
    preparedOversampler.setOversampleFactor(2);
    preparedOversampler.setMemoryBuffer({ oversampledImpulse.data(), (int) oversampledImpulse.size() });
    Buffer<float> rawImpulseBuffer(rawImpulse.data(), (int) rawImpulse.size());
    CycleDsp::rasterizeIrImpulse(
            curve.sampler(),
            rawImpulseBuffer,
            preparedOversampler,
            kIrPadding);

    Transform transform;
    transform.allocate((int) impulseLength, Transform::DivFwdByN, true);
    Buffer<float> levels(prefilterLevels.data(), (int) prefilterLevels.size());
    CycleDsp::buildIrPrefilterLevels(levels, highPass);
    CycleDsp::applyIrFrequencyPrefilter(
            rawImpulseBuffer,
            { result->impulse.data(), (int) result->impulse.size() },
            levels,
            transform);
    result->postGain = CycleDsp::irPostGain(parameterFloat(parameters, "post", 0.5f));
    return result;
}

void IrSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    if (configuration == nullptr || spec.maximumFrameCount == 0) {
        return;
    }

    prepareConvolver(blockConvolver, spec.maximumFrameCount, preparedBlockSize);
    prepareConvolver(traversalConvolver, spec.maximumFrameCount, preparedTraversalSize);
    convolutionOutput.resize(spec.maximumFrameCount);
}

void IrSignalProcessor::adoptConfiguration(const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::ImpulseResponse) {
        return;
    }

    configuration = std::static_pointer_cast<const IrConfiguration>(published.value);
    postGain = configuration->postGain;
    preparedBlockSize = 0;
    preparedTraversalSize = 0;
    adoptedRevision = published.revision;
}

void IrSignalProcessor::beginBlock(size_t frameCount) {
    ignoreUnused(frameCount);
    activeConvolver = &blockConvolver;
}

void IrSignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    activeConvolver = &traversalConvolver;
    if (configuration == nullptr || preparedTraversalSize != rows) {
        preparedTraversalSize = 0;
        prepareConvolver(traversalConvolver, rows, preparedTraversalSize);
    }
}

void IrSignalProcessor::endTraversalGrid() {
    activeConvolver = &blockConvolver;
}

void IrSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (buffer.empty() || configuration == nullptr || configuration->impulse.empty()
            || activeConvolver == nullptr) {
        return;
    }

    convolutionOutput.resize((size_t) buffer.size());
    activeConvolver->process(
            buffer,
            { convolutionOutput.data(), buffer.size() });

    VecOps::copy(convolutionOutput.data(), buffer.get(), buffer.size());

    buffer.mul(postGain);
}

void IrSignalProcessor::prepareConvolver(
        BlockConvolver& convolver,
        size_t blockSize,
        size_t& preparedSize) {
    if (configuration == nullptr || blockSize == 0 || preparedSize == blockSize) {
        return;
    }

    convolver.init(
            NumberUtils::nextPower2((unsigned) blockSize),
            Buffer<float>(
                    const_cast<float*>(configuration->impulse.data()),
                    (int) configuration->impulse.size()));
    preparedSize = blockSize;
    convolutionOutput.resize(blockSize);
}

void DelaySignalProcessor::configure(
        const std::vector<NodeParameter>& parametersToUse,
        const AudioProcessTiming& timing) {
    DelayConfiguration prepared;
    prepared.time = parameterFloat(parametersToUse, "time", 0.5f);
    prepared.feedback = parameterFloat(parametersToUse, "feedback", 0.5f);
    prepared.spin = parameterFloat(parametersToUse, "spin", 1.f);
    prepared.wet = parameterFloat(parametersToUse, "wet", 0.9f);
    prepared.spinIterations = parameterFloat(parametersToUse, "spinIters", 0.f);
    configure(prepared, timing);
}

void DelaySignalProcessor::configure(
        const DelayConfiguration& prepared,
        const AudioProcessTiming& timing) {
    bpm = std::max(1.0, timing.bpm);
    beatsPerMeasure = std::max(1, timing.beatsPerMeasure);

    CycleDsp::DelayConfiguration configuration;
    configuration.sampleRate = std::max(1.0, timing.sampleRate);
    configuration.delaySeconds = CycleDsp::delayTimeSeconds(
            prepared.time,
            bpm,
            beatsPerMeasure);
    configuration.feedback = jlimit(
            0.f,
            0.98f,
            prepared.feedback);
    configuration.spin = jlimit(0.f, 1.f, prepared.spin);
    configuration.wet = jlimit(0.f, 1.f, prepared.wet);
    configuration.spinIterations = CycleDsp::delaySpinIterations(
            prepared.spinIterations);

    blockDelay.configure(configuration);
    traversalDelay.configure(configuration);
}

void DelaySignalProcessor::beginBlock(size_t) {
    activeDelay = &blockDelay;
}

void DelaySignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    (void) rows;
    activeDelay = &traversalDelay;
    traversalDelay.reset();
}

void DelaySignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (activeDelay == nullptr) {
        return;
    }
    activeDelay->process(buffer);
}

std::shared_ptr<const ReverbConfiguration> ReverbSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters) {
    auto result = std::make_shared<ReverbConfiguration>();
    result->enabled = typedParameterBool(parameters, "enabled", true);
    const float roomSize = jlimit(0.f, 1.f, parameterFloat(parameters, "size", 0.35f));
    const float damping = jlimit(0.f, 1.f, parameterFloat(parameters, "damp", 0.2f)) * 0.7f;
    const float highPass = jlimit(0.f, 1.f, parameterFloat(parameters, "highPass", 0.05f));
    const size_t kernelLength = (size_t) NumberUtils::nextPower2(
            (unsigned) std::pow(2.f, 12.f + 6.f * roomSize));

    result->kernel.assign(kernelLength, 0.f);
    result->width = jlimit(0.f, 1.f, parameterFloat(parameters, "width", 0.75f));
    result->wetLevel = 0.25f * jlimit(0.f, 1.f, parameterFloat(parameters, "wet", 0.5f));

    CycleDsp::ReverbKernelConfiguration kernelConfiguration;
    kernelConfiguration.roomSize = roomSize;
    kernelConfiguration.damping = damping;
    kernelConfiguration.highPass = highPass;
    CycleDsp::buildReverbKernel(
            kernelConfiguration,
            { result->kernel.data(), (int) result->kernel.size() });
    return result;
}

void ReverbSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    if (configuration == nullptr || spec.maximumFrameCount == 0) {
        return;
    }

    prepareConvolver(blockConvolver, spec.maximumFrameCount, preparedBlockSize);
    prepareConvolver(traversalConvolver, spec.maximumFrameCount, preparedTraversalSize);
    dryBuffer.resize(spec.maximumFrameCount);
    convolutionOutput.resize(spec.maximumFrameCount);
}

void ReverbSignalProcessor::adoptConfiguration(const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::Reverb) {
        return;
    }

    configuration = std::static_pointer_cast<const ReverbConfiguration>(published.value);
    wetLevel = configuration->wetLevel;
    preparedBlockSize = 0;
    preparedTraversalSize = 0;
    adoptedRevision = published.revision;
}

void ReverbSignalProcessor::beginBlock(size_t frameCount) {
    ignoreUnused(frameCount);
    activeConvolver = &blockConvolver;
}

void ReverbSignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    activeConvolver = &traversalConvolver;
    if (configuration == nullptr || preparedTraversalSize != rows) {
        preparedTraversalSize = 0;
        prepareConvolver(traversalConvolver, rows, preparedTraversalSize);
    }
}

void ReverbSignalProcessor::endTraversalGrid() {
    activeConvolver = &blockConvolver;
}

void ReverbSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (buffer.empty() || configuration == nullptr || configuration->kernel.empty()
            || activeConvolver == nullptr) {
        return;
    }

    dryBuffer.resize((size_t) buffer.size());
    convolutionOutput.resize((size_t) buffer.size());
    VecOps::copy(buffer.get(), dryBuffer.data(), buffer.size());
    activeConvolver->process(
            buffer,
            { convolutionOutput.data(), buffer.size() });

    const float dryScale = 1.f - 0.25f * wetLevel;
    buffer.mul(0.f);
    buffer.addProduct({ dryBuffer.data(), buffer.size() }, dryScale);
    buffer.addProduct({ convolutionOutput.data(), buffer.size() }, wetLevel);
}

void ReverbSignalProcessor::prepareConvolver(
        ConvReverb& convolver,
        size_t blockSize,
        size_t& preparedSize) {
    if (configuration == nullptr || blockSize == 0 || preparedSize == blockSize) {
        return;
    }

    const int headSize = NumberUtils::nextPower2((unsigned) blockSize);
    convolver.init(
            headSize,
            16 * headSize,
            Buffer<float>(
                    const_cast<float*>(configuration->kernel.data()),
                    (int) configuration->kernel.size()));
    preparedSize = blockSize;
    dryBuffer.resize(blockSize);
    convolutionOutput.resize(blockSize);
}

}
