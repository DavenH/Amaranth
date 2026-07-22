#include "EffectSignalProcessors.h"

#include "../../Graph/NodeDefinition.h"
#include "../Effect2D/FlatCurvePreparation.h"

#include <Algo/ConvReverb.h>
#include <Algo/FFT.h>
#include <Array/ScopedAlloc.h>
#include <Audio/CycleDsp/ReverbKernel.h>
#include <Audio/CycleDsp/EffectParameterMapping.h>
#include <Util/NumberUtils.h>

#include <algorithm>
#include <cmath>

namespace CycleV2 {

namespace {

constexpr float kIrPadding = 0.0625f;

}

std::shared_ptr<const IrConfiguration> IrSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model) {
    auto result = std::make_shared<IrConfiguration>();
    result->enabled = typedParameterBool(parameters, "enabled", true);
    const float highPass = parameterFloat(parameters, "highPass", 0.5f);
    const size_t impulseLength = (size_t) CycleDsp::irImpulseLength(
            parameterFloat(parameters, "size", 0.5f));
    FlatCurvePreparation curve(
            "CycleV2IrConfiguration",
            NodeKind::ImpulseResponse,
            parameters,
            model,
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

    configuration.channel = CycleDsp::DelayChannel::Left;
    blockDelays[0].configure(configuration);
    traversalDelays[0].configure(configuration);
    configuration.channel = CycleDsp::DelayChannel::Right;
    blockDelays[1].configure(configuration);
    traversalDelays[1].configure(configuration);
}

void DelaySignalProcessor::beginBlock(size_t) {
    processingTraversal = false;
}

void DelaySignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    (void) rows;
    processingTraversal = true;
    traversalDelays[0].reset();
    traversalDelays[1].reset();
}

void DelaySignalProcessor::processBuffer(
        Buffer<float> buffer,
        const SignalProcessPosition& position) {
    const size_t channel = std::min<size_t>(position.channel, 1);
    auto& delay = processingTraversal
            ? traversalDelays[channel]
            : blockDelays[channel];
    delay.process(buffer);
}

std::shared_ptr<const ReverbConfiguration> ReverbSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters) {
    auto result = std::make_shared<ReverbConfiguration>();
    result->enabled = typedParameterBool(parameters, "enabled", true);
    const float roomSize = jlimit(0.f, 1.f, parameterFloat(parameters, "size", 0.5f));
    const float damping = CycleDsp::reverbDamping(parameterFloat(parameters, "damp", 0.2f));
    const float highPass = jlimit(0.f, 1.f, parameterFloat(parameters, "highPass", 0.05f));
    const size_t kernelLength = CycleDsp::reverbKernelLength(roomSize);

    result->kernels[0].assign(kernelLength, 0.f);
    result->kernels[1].assign(kernelLength, 0.f);
    result->width = jlimit(0.f, 1.f, parameterFloat(parameters, "width", 1.f));
    result->wetLevel = CycleDsp::reverbWetLevel(parameterFloat(parameters, "wet", 0.4f));

    CycleDsp::ReverbKernelConfiguration kernelConfiguration;
    kernelConfiguration.roomSize = roomSize;
    kernelConfiguration.damping = damping;
    kernelConfiguration.highPass = highPass;
    CycleDsp::buildReverbKernel(
            kernelConfiguration,
            { result->kernels[0].data(), (int) result->kernels[0].size() },
            { result->kernels[1].data(), (int) result->kernels[1].size() });
    return result;
}

void ReverbSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    if (configuration == nullptr || spec.maximumFrameCount == 0) {
        return;
    }

    prepareBlockConvolver(spec.maximumFrameCount);
    prepareTraversalConvolver(spec.maximumFrameCount);
    for (size_t channel = 0; channel < convolvers.size(); ++channel) {
        dryBuffers[channel].resize(spec.maximumFrameCount);
        mixBuffers[channel].resize(spec.maximumFrameCount);
        convolvers[channel].prepareScratch(spec.maximumFrameCount);
    }
}

void ReverbSignalProcessor::adoptConfiguration(const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::Reverb) {
        return;
    }

    configuration = std::static_pointer_cast<const ReverbConfiguration>(published.value);
    wetLevel = configuration->wetLevel;
    for (auto& channelConvolvers : convolvers) {
        channelConvolvers.invalidate();
    }
    adoptedRevision = published.revision;
}

void ReverbSignalProcessor::beginBlock(size_t frameCount) {
    beginChannelBlock(1, frameCount);
}

void ReverbSignalProcessor::beginChannelBlock(size_t channelCount, size_t frameCount) {
    ignoreUnused(frameCount);
    activeChannelCount = std::min<size_t>(channelCount, 2);
    pendingWet = {};
    for (auto& channelConvolvers : convolvers) {
        channelConvolvers.beginBlock();
    }
}

void ReverbSignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    beginChannelTraversalGrid(1, 0, rows);
}

void ReverbSignalProcessor::beginChannelTraversalGrid(
        size_t channelCount,
        size_t columns,
        size_t rows) {
    ignoreUnused(columns);
    activeChannelCount = std::min<size_t>(channelCount, 2);
    pendingWet = {};
    for (size_t channel = 0; channel < convolvers.size(); ++channel) {
        convolvers[channel].beginTraversal();
        prepareConvolver(channel, convolvers[channel].traversal(), rows);
        convolvers[channel].markTraversalPrepared(rows);
    }
}

void ReverbSignalProcessor::endTraversalGrid() {
    for (auto& channelConvolvers : convolvers) {
        channelConvolvers.endTraversal();
    }
}

void ReverbSignalProcessor::processBuffer(
        Buffer<float> buffer,
        const SignalProcessPosition& position) {
    const size_t channel = std::min<size_t>(position.channel, 1);
    auto& channelConvolvers = convolvers[channel];
    if (buffer.empty() || configuration == nullptr || configuration->kernels[channel].empty()
            || channelConvolvers.active() == nullptr) {
        return;
    }

    if ((size_t) buffer.size() > dryBuffers[channel].size()) {
        return;
    }

    Buffer<float> convolutionOutput = channelConvolvers.output((size_t) buffer.size());
    if (convolutionOutput.empty()) {
        return;
    }

    VecOps::copy(buffer.get(), dryBuffers[channel].data(), buffer.size());
    channelConvolvers.active()->process(
            buffer,
            convolutionOutput);
    convolutionOutput.copyTo(buffer);
    pendingWet[channel] = buffer;
    if (activeChannelCount == 1 || channel + 1 == activeChannelCount) {
        mixPendingBuffers((size_t) buffer.size());
    }
}

void ReverbSignalProcessor::prepareBlockConvolver(size_t blockSize) {
    for (size_t channel = 0; channel < convolvers.size(); ++channel) {
        if (configuration == nullptr || blockSize == 0
                || !convolvers[channel].blockNeedsPreparation(blockSize)) {
            continue;
        }
        prepareConvolver(channel, convolvers[channel].block(), blockSize);
        convolvers[channel].markBlockPrepared(blockSize);
    }
}

void ReverbSignalProcessor::prepareTraversalConvolver(size_t rowCount) {
    for (size_t channel = 0; channel < convolvers.size(); ++channel) {
        if (configuration == nullptr || rowCount == 0
                || !convolvers[channel].traversalNeedsPreparation(rowCount)) {
            continue;
        }
        prepareConvolver(channel, convolvers[channel].traversal(), rowCount);
        convolvers[channel].markTraversalPrepared(rowCount);
    }
}

void ReverbSignalProcessor::prepareConvolver(
        size_t channel,
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
                    const_cast<float*>(configuration->kernels[channel].data()),
                    (int) configuration->kernels[channel].size()));
    dryBuffers[channel].resize(frameCount);
    mixBuffers[channel].resize(frameCount);
    convolvers[channel].prepareScratch(frameCount);
}

void ReverbSignalProcessor::mixPendingBuffers(size_t frameCount) {
    if (pendingWet[0].empty()) {
        return;
    }

    if (activeChannelCount == 1 || pendingWet[1].empty()) {
        const float dryScale = 1.f - 0.25f * wetLevel;
        pendingWet[0].mul(wetLevel);
        pendingWet[0].addProduct({ dryBuffers[0].data(), (int) frameCount }, dryScale);
        return;
    }

    const float direct = wetLevel * jmax(0.5f, configuration->width);
    const float cross = wetLevel * jmin(0.5f, 1.f - configuration->width);
    const float dryScale = 1.f - 0.24f * wetLevel;
    Buffer<float> leftMix(mixBuffers[0].data(), (int) frameCount);
    Buffer<float> rightMix(mixBuffers[1].data(), (int) frameCount);
    VecOps::mul(pendingWet[0], direct, leftMix);
    leftMix.addProduct(pendingWet[1], cross);
    leftMix.addProduct({ dryBuffers[0].data(), (int) frameCount }, dryScale);
    VecOps::mul(pendingWet[1], direct, rightMix);
    rightMix.addProduct(pendingWet[0], cross);
    rightMix.addProduct({ dryBuffers[1].data(), (int) frameCount }, dryScale);
    leftMix.copyTo(pendingWet[0]);
    rightMix.copyTo(pendingWet[1]);
}

std::shared_ptr<const EqualizerConfiguration> EqualizerSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters) {
    auto result = std::make_shared<EqualizerConfiguration>();
    result->enabled = typedParameterBool(parameters, "enabled", true);
    for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
        const String number(band + 1);
        result->gains[(size_t) band] = CycleDsp::equalizerGainDecibels(
                parameterFloat(parameters, "band" + number + "Gain", 0.5f));
        result->frequencies[(size_t) band] = CycleDsp::equalizerFrequency(
                parameterFloat(parameters, "band" + number + "Frequency", 0.5f));
    }
    return result;
}

EqualizerSignalProcessor::EqualizerSignalProcessor() : blockCore(2), traversalCore(2) {}

void EqualizerSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    sampleRate = std::max(1.0, spec.sampleRate);
    configure(blockCore);
    configure(traversalCore);
}

void EqualizerSignalProcessor::adoptConfiguration(
        const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::Equalizer) {
        return;
    }

    configuration = std::static_pointer_cast<const EqualizerConfiguration>(published.value);
    adoptedRevision = published.revision;
}

void EqualizerSignalProcessor::beginBlock(size_t) {
    processingTraversal = false;
}

void EqualizerSignalProcessor::beginTraversalGrid(size_t, size_t) {
    processingTraversal = true;
    traversalCore.clear();
}

void EqualizerSignalProcessor::processBuffer(
        Buffer<float> buffer,
        const SignalProcessPosition& position) {
    auto& core = processingTraversal ? traversalCore : blockCore;
    core.process((int) std::min<size_t>(position.channel, 1), buffer);
}

void EqualizerSignalProcessor::configure(CycleDsp::EqualizerCore& core) {
    if (configuration == nullptr) {
        return;
    }

    for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
        core.configureBand(
                band,
                sampleRate,
                configuration->frequencies[(size_t) band],
                configuration->gains[(size_t) band]);
    }
}

}
