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

    prepareBlockConvolver(spec.maximumFrameCount);
    prepareTraversalConvolver(spec.maximumFrameCount);
    convolvers.prepareScratch(spec.maximumFrameCount);
}

void IrSignalProcessor::adoptConfiguration(const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::ImpulseResponse) {
        return;
    }

    configuration = std::static_pointer_cast<const IrConfiguration>(published.value);
    postGain = configuration->postGain;
    convolvers.invalidate();
    adoptedRevision = published.revision;
}

void IrSignalProcessor::beginBlock(size_t frameCount) {
    ignoreUnused(frameCount);
    convolvers.beginBlock();
}

void IrSignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    convolvers.beginTraversal();
    prepareConvolver(convolvers.traversal(), rows);
    convolvers.markTraversalPrepared(rows);
}

void IrSignalProcessor::endTraversalGrid() {
    convolvers.endTraversal();
}

void IrSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (buffer.empty() || configuration == nullptr || configuration->impulse.empty()
            || convolvers.active() == nullptr) {
        return;
    }

    Buffer<float> convolutionOutput = convolvers.output((size_t) buffer.size());
    if (convolutionOutput.empty()) {
        return;
    }

    convolvers.active()->process(
            buffer,
            convolutionOutput);

    VecOps::copy(convolutionOutput.get(), buffer.get(), buffer.size());

    buffer.mul(postGain);
}

void IrSignalProcessor::prepareBlockConvolver(size_t blockSize) {
    if (configuration == nullptr || blockSize == 0
            || !convolvers.blockNeedsPreparation(blockSize)) {
        return;
    }

    prepareConvolver(convolvers.block(), blockSize);
    convolvers.markBlockPrepared(blockSize);
}

void IrSignalProcessor::prepareTraversalConvolver(size_t rowCount) {
    if (configuration == nullptr || rowCount == 0
            || !convolvers.traversalNeedsPreparation(rowCount)) {
        return;
    }

    prepareConvolver(convolvers.traversal(), rowCount);
    convolvers.markTraversalPrepared(rowCount);
}

void IrSignalProcessor::prepareConvolver(
        BlockConvolver& convolver,
        size_t frameCount) {
    if (configuration == nullptr || frameCount == 0) {
        return;
    }

    convolver.init(
            NumberUtils::nextPower2((unsigned) frameCount),
            Buffer<float>(
                    const_cast<float*>(configuration->impulse.data()),
                    (int) configuration->impulse.size()));
    convolvers.prepareScratch(frameCount);
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

    prepareBlockConvolver(spec.maximumFrameCount);
    prepareTraversalConvolver(spec.maximumFrameCount);
    dryBuffer.resize(spec.maximumFrameCount);
    convolvers.prepareScratch(spec.maximumFrameCount);
}

void ReverbSignalProcessor::adoptConfiguration(const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::Reverb) {
        return;
    }

    configuration = std::static_pointer_cast<const ReverbConfiguration>(published.value);
    wetLevel = configuration->wetLevel;
    convolvers.invalidate();
    adoptedRevision = published.revision;
}

void ReverbSignalProcessor::beginBlock(size_t frameCount) {
    ignoreUnused(frameCount);
    convolvers.beginBlock();
}

void ReverbSignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    convolvers.beginTraversal();
    prepareConvolver(convolvers.traversal(), rows);
    convolvers.markTraversalPrepared(rows);
}

void ReverbSignalProcessor::endTraversalGrid() {
    convolvers.endTraversal();
}

void ReverbSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (buffer.empty() || configuration == nullptr || configuration->kernel.empty()
            || convolvers.active() == nullptr) {
        return;
    }

    if ((size_t) buffer.size() > dryBuffer.size()) {
        return;
    }

    Buffer<float> convolutionOutput = convolvers.output((size_t) buffer.size());
    if (convolutionOutput.empty()) {
        return;
    }

    VecOps::copy(buffer.get(), dryBuffer.data(), buffer.size());
    convolvers.active()->process(
            buffer,
            convolutionOutput);

    const float dryScale = 1.f - 0.25f * wetLevel;
    buffer.mul(0.f);
    buffer.addProduct({ dryBuffer.data(), buffer.size() }, dryScale);
    buffer.addProduct(convolutionOutput, wetLevel);
}

void ReverbSignalProcessor::prepareBlockConvolver(size_t blockSize) {
    if (configuration == nullptr || blockSize == 0
            || !convolvers.blockNeedsPreparation(blockSize)) {
        return;
    }

    prepareConvolver(convolvers.block(), blockSize);
    convolvers.markBlockPrepared(blockSize);
}

void ReverbSignalProcessor::prepareTraversalConvolver(size_t rowCount) {
    if (configuration == nullptr || rowCount == 0
            || !convolvers.traversalNeedsPreparation(rowCount)) {
        return;
    }

    prepareConvolver(convolvers.traversal(), rowCount);
    convolvers.markTraversalPrepared(rowCount);
}

void ReverbSignalProcessor::prepareConvolver(
        ConvReverb& convolver,
        size_t frameCount) {
    if (configuration == nullptr || frameCount == 0) {
        return;
    }

    const int headSize = NumberUtils::nextPower2((unsigned) frameCount);
    convolver.init(
            headSize,
            16 * headSize,
            Buffer<float>(
                    const_cast<float*>(configuration->kernel.data()),
                    (int) configuration->kernel.size()));
    dryBuffer.resize(frameCount);
    convolvers.prepareScratch(frameCount);
}

}
