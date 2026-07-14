#include "EffectSignalProcessors.h"

#include "../../Graph/NodeDefinition.h"
#include "../Effect2D/CurveNodeModels.h"
#include "../Effect2D/Effect2DMeshState.h"

#include <Algo/ConvReverb.h>
#include <Algo/FFT.h>
#include <Array/ScopedAlloc.h>
#include <Audio/CycleDsp/ReverbKernel.h>
#include <Curve/Curve.h>
#include <Curve/Mesh/Vertex.h>
#include <Util/NumberUtils.h>

#include <algorithm>
#include <cmath>

namespace CycleV2 {

namespace {

constexpr float kIrPadding = 0.0625f;

void ensureCurveTable() {
    if (Curve::table == nullptr) {
        Curve::calcTable();
    }
}

std::vector<Effect2DVertexState> defaultIrVertices() {
    return {
            { 0.f, 0.5f, 1.f },
            { kIrPadding, 0.95f, 0.35f },
            { kIrPadding * 2.f, 0.3f, 0.45f },
            { kIrPadding * 3.f, 0.55f, 0.7f },
            { 1.f, 0.5f, 1.f }
    };
}

}

IrSignalProcessor::IrSignalProcessor() {
    rasterizer.setDims(Dimensions(Vertex::Phase, Vertex::Amp));
    rasterizer.setScalingMode(FXRasterizer::Bipolar);
    rasterizer.setMesh(&mesh);
    impulseOversampler.setOversampleFactor(2);
}

IrSignalProcessor::~IrSignalProcessor() {
    mesh.destroy();
}

std::shared_ptr<const IrConfiguration> IrSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters) {
    auto result = std::make_shared<IrConfiguration>();
    result->enabled = typedParameterBool(parameters, "enabled", true);
    const float highPass = parameterFloat(parameters, "highPass", 0.5f);
    const size_t impulseLength = (size_t) CycleDsp::irImpulseLength(
            parameterFloat(parameters, "size", 0.5f));
    Mesh preparedMesh("CycleV2IrConfiguration");
    FXRasterizer preparedRasterizer(nullptr, "CycleV2IrConfigurationRasterizer");
    preparedRasterizer.setDims(Dimensions(Vertex::Phase, Vertex::Amp));
    preparedRasterizer.setScalingMode(FXRasterizer::Bipolar);
    preparedRasterizer.setMesh(&preparedMesh);

    auto vertices = CurveNodeModelCodec::flatVerticesFromParameters(parameters);
    if (vertices.empty()) {
        vertices = defaultIrVertices();
    }

    for (const auto& state : vertices) {
        auto* vertex = new Vertex(state.x, state.y);
        vertex->values[Vertex::Curve] = state.curve;
        if (state.curve >= 1.f) {
            vertex->setMaxSharpness();
        }
        preparedMesh.addVertex(vertex);
    }

    ensureCurveTable();
    preparedRasterizer.updateGeometry();
    preparedRasterizer.updateWaveform();

    result->impulse.resize(impulseLength);
    std::vector<float> rawImpulse(impulseLength);
    std::vector<float> oversampledImpulse(impulseLength * 2);
    std::vector<float> prefilterLevels(impulseLength / 2);
    Oversampler preparedOversampler(8);
    preparedOversampler.setOversampleFactor(2);
    preparedOversampler.setMemoryBuffer({ oversampledImpulse.data(), (int) oversampledImpulse.size() });
    Buffer<float> rawImpulseBuffer(rawImpulse.data(), (int) rawImpulse.size());
    if (preparedRasterizer.canRasterizeWaveform()) {
        CycleDsp::rasterizeIrImpulse(
                preparedRasterizer.sampler(),
                rawImpulseBuffer,
                preparedOversampler,
                kIrPadding);
    } else {
        rawImpulseBuffer.zero();
        rawImpulseBuffer.front() = 1.f;
    }

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
    preparedMesh.destroy();
    return result;
}

void IrSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    if (configuration == nullptr || spec.maximumFrameCount == 0) {
        return;
    }

    prepareConvolver(blockConvolver, spec.maximumFrameCount, preparedBlockSize);
    blockImpulseRevision = impulseRevision;
    prepareConvolver(traversalConvolver, spec.maximumFrameCount, preparedTraversalSize);
    traversalImpulseRevision = impulseRevision;
    convolutionOutput.resize(spec.maximumFrameCount);
}

void IrSignalProcessor::adoptConfiguration(const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::ImpulseResponse) {
        return;
    }

    configuration = std::static_pointer_cast<const IrConfiguration>(published.value);
    postGain = configuration->postGain;
    impulseRevision = (size_t) published.revision;
    preparedBlockSize = 0;
    preparedTraversalSize = 0;
    adoptedRevision = published.revision;
}

void IrSignalProcessor::prepareLegacy(
        const std::vector<NodeParameter>& parametersToUse,
        const AudioProcessTiming&) {
    if (configuration != nullptr) {
        return;
    }

    postGain = CycleDsp::irPostGain(parameterFloat(parametersToUse, "post", 0.5f));
    highPass = parameterFloat(parametersToUse, "highPass", 0.5f);
    impulseLength = (size_t) CycleDsp::irImpulseLength(parameterFloat(parametersToUse, "size", 0.5f));
    syncImpulse(parametersToUse);
}

void IrSignalProcessor::beginBlock(size_t frameCount) {
    activeConvolver = &blockConvolver;
    if (configuration == nullptr) {
        prepareConvolver(blockConvolver, frameCount, preparedBlockSize);
    }
    blockImpulseRevision = impulseRevision;
}

void IrSignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    activeConvolver = &traversalConvolver;
    if (configuration == nullptr || preparedTraversalSize != rows) {
        preparedTraversalSize = 0;
        prepareConvolver(traversalConvolver, rows, preparedTraversalSize);
    }
    traversalImpulseRevision = impulseRevision;
}

void IrSignalProcessor::endTraversalGrid() {
    activeConvolver = &blockConvolver;
}

void IrSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    const bool hasImpulse = configuration != nullptr ? !configuration->impulse.empty() : !impulse.empty();
    if (buffer.empty() || !hasImpulse || activeConvolver == nullptr) {
        return;
    }

    convolutionOutput.resize((size_t) buffer.size());
    activeConvolver->process(
            buffer,
            { convolutionOutput.data(), buffer.size() });

    VecOps::copy(convolutionOutput.data(), buffer.get(), buffer.size());

    buffer.mul(postGain);
}

void IrSignalProcessor::syncImpulse(const std::vector<NodeParameter>& parametersToUse) {
    const String serializedVertices = Effect2DMeshState::serialize(
            CurveNodeModelCodec::flatVerticesFromParameters(parametersToUse));
    if (serializedVertices == lastVertexState
            && impulse.size() == impulseLength
            && impulseHighPass == highPass) {
        return;
    }

    rebuildMesh(serializedVertices);
    lastVertexState = serializedVertices;
    ensureCurveTable();

    const bool lengthChanged = impulse.size() != impulseLength;
    impulse.resize(impulseLength);
    rawImpulse.resize(impulseLength);
    oversampledImpulse.resize(impulseLength * (size_t) impulseOversampler.getOversampleFactor());
    prefilterLevels.resize(impulseLength / 2);
    if (lengthChanged) {
        impulseTransform.allocate((int) impulseLength, Transform::DivFwdByN, true);
    }
    rasterizer.updateGeometry();
    rasterizer.updateWaveform();

    Buffer<float> rawImpulseBuffer(rawImpulse.data(), (int) rawImpulse.size());
    if (rasterizer.canRasterizeWaveform()) {
        Buffer<float> oversampledBuffer(
                oversampledImpulse.data(),
                (int) oversampledImpulse.size());
        impulseOversampler.setMemoryBuffer(oversampledBuffer);
        CycleDsp::rasterizeIrImpulse(
                rasterizer.sampler(),
                rawImpulseBuffer,
                impulseOversampler,
                kIrPadding);
    } else {
        rawImpulseBuffer.zero();
        rawImpulseBuffer.front() = 1.f;
    }

    Buffer<float> levelBuffer(prefilterLevels.data(), (int) prefilterLevels.size());
    CycleDsp::buildIrPrefilterLevels(levelBuffer, highPass);
    CycleDsp::applyIrFrequencyPrefilter(
            rawImpulseBuffer,
            { impulse.data(), (int) impulse.size() },
            levelBuffer,
            impulseTransform);
    impulseHighPass = highPass;
    ++impulseRevision;
}

void IrSignalProcessor::prepareConvolver(
        BlockConvolver& convolver,
        size_t blockSize,
        size_t& preparedSize) {
    const bool isBlockConvolver = &convolver == &blockConvolver;
    const size_t preparedRevision = isBlockConvolver
            ? blockImpulseRevision
            : traversalImpulseRevision;
    if (blockSize == 0
            || (preparedSize == blockSize && preparedRevision == impulseRevision)) {
        return;
    }

    convolver.init(
            NumberUtils::nextPower2((unsigned) blockSize),
            configuration != nullptr
                    ? Buffer<float>(
                            const_cast<float*>(configuration->impulse.data()),
                            (int) configuration->impulse.size())
                    : Buffer<float>(impulse.data(), (int) impulse.size()));
    preparedSize = blockSize;
    convolutionOutput.resize(blockSize);
}

void IrSignalProcessor::rebuildMesh(const String& serializedVertices) {
    mesh.destroy();

    auto vertices = Effect2DMeshState::parse(serializedVertices);
    if (vertices.empty()) {
        vertices = defaultIrVertices();
    }

    for (const auto& vertex : vertices) {
        addVertex(vertex.x, vertex.y, vertex.curve);
    }
}

void IrSignalProcessor::addVertex(float x, float y, float curve) {
    auto* vertex = new Vertex(x, y);
    vertex->values[Vertex::Curve] = curve;

    if (curve >= 1.f) {
        vertex->setMaxSharpness();
    }

    mesh.addVertex(vertex);
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

void ReverbSignalProcessor::prepareLegacy(
        const std::vector<NodeParameter>& parametersToUse,
        const AudioProcessTiming&) {
    if (configuration != nullptr) {
        return;
    }

    roomSize = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "size", 0.35f));
    rolloffFactor = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "damp", 0.2f)) * 0.7f;
    width = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "width", 0.75f));
    highPass = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "highPass", 0.05f));
    wetLevel = 0.25f * jlimit(0.f, 1.f, parameterFloat(parametersToUse, "wet", 0.5f));
    kernelLength = (size_t) NumberUtils::nextPower2((unsigned) std::pow(2.f, 12.f + 6.f * roomSize));
    syncKernel();
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
    blockKernelRevision = kernelRevision;
    prepareConvolver(traversalConvolver, spec.maximumFrameCount, preparedTraversalSize);
    traversalKernelRevision = kernelRevision;
    dryBuffer.resize(spec.maximumFrameCount);
    convolutionOutput.resize(spec.maximumFrameCount);
}

void ReverbSignalProcessor::adoptConfiguration(const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::Reverb) {
        return;
    }

    configuration = std::static_pointer_cast<const ReverbConfiguration>(published.value);
    width = configuration->width;
    wetLevel = configuration->wetLevel;
    kernelRevision = (size_t) published.revision;
    preparedBlockSize = 0;
    preparedTraversalSize = 0;
    adoptedRevision = published.revision;
}

void ReverbSignalProcessor::beginBlock(size_t frameCount) {
    activeConvolver = &blockConvolver;
    if (configuration == nullptr) {
        prepareConvolver(blockConvolver, frameCount, preparedBlockSize);
    }
    blockKernelRevision = kernelRevision;
}

void ReverbSignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    activeConvolver = &traversalConvolver;
    if (configuration == nullptr || preparedTraversalSize != rows) {
        preparedTraversalSize = 0;
        prepareConvolver(traversalConvolver, rows, preparedTraversalSize);
    }
    traversalKernelRevision = kernelRevision;
}

void ReverbSignalProcessor::endTraversalGrid() {
    activeConvolver = &blockConvolver;
}

void ReverbSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    const bool hasKernel = configuration != nullptr ? !configuration->kernel.empty() : !kernel.empty();
    if (buffer.empty() || !hasKernel || activeConvolver == nullptr) {
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

void ReverbSignalProcessor::syncKernel() {
    const String signature = String(roomSize)
            + ":" + String(rolloffFactor)
            + ":" + String(highPass)
            + ":" + String((int) kernelLength);

    if (signature == kernelSignature && kernel.size() == kernelLength) {
        return;
    }

    kernelSignature = signature;
    kernel.assign(kernelLength, 0.f);
    ++kernelRevision;

    CycleDsp::ReverbKernelConfiguration configuration;
    configuration.roomSize = roomSize;
    configuration.damping = rolloffFactor;
    configuration.highPass = highPass;
    CycleDsp::buildReverbKernel(
            configuration,
            { kernel.data(), (int) kernel.size() });
}

void ReverbSignalProcessor::prepareConvolver(
        ConvReverb& convolver,
        size_t blockSize,
        size_t& preparedSize) {
    const bool isBlockConvolver = &convolver == &blockConvolver;
    const size_t preparedRevision = isBlockConvolver
            ? blockKernelRevision
            : traversalKernelRevision;
    if (blockSize == 0
            || (preparedSize == blockSize && preparedRevision == kernelRevision)) {
        return;
    }

    const int headSize = NumberUtils::nextPower2((unsigned) blockSize);
    convolver.init(
            headSize,
            16 * headSize,
            configuration != nullptr
                    ? Buffer<float>(
                            const_cast<float*>(configuration->kernel.data()),
                            (int) configuration->kernel.size())
                    : Buffer<float>(kernel.data(), (int) kernel.size()));
    preparedSize = blockSize;
    dryBuffer.resize(blockSize);
    convolutionOutput.resize(blockSize);
}

}
