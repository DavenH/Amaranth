#include "EffectSignalProcessors.h"

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

String parameterValue(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        const String& fallback = {}) {
    for (const auto& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value;
        }
    }

    return fallback;
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

void IrSignalProcessor::prepareProcess(
        const std::vector<NodeParameter>& parametersToUse,
        const AudioProcessTiming&) {
    postGain = CycleDsp::irPostGain(parameterFloat(parametersToUse, "post", 0.5f));
    highPass = parameterFloat(parametersToUse, "highPass", 0.5f);
    impulseLength = (size_t) CycleDsp::irImpulseLength(parameterFloat(parametersToUse, "size", 0.5f));
    syncImpulse(parametersToUse);
}

void IrSignalProcessor::beginBlock(size_t frameCount) {
    activeConvolver = &blockConvolver;
    prepareConvolver(blockConvolver, frameCount, preparedBlockSize);
    blockImpulseRevision = impulseRevision;
}

void IrSignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    activeConvolver = &traversalConvolver;
    preparedTraversalSize = 0;
    prepareConvolver(traversalConvolver, rows, preparedTraversalSize);
    traversalImpulseRevision = impulseRevision;
}

void IrSignalProcessor::endTraversalGrid() {
    activeConvolver = &blockConvolver;
}

void IrSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (buffer.empty() || impulse.empty() || activeConvolver == nullptr) {
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
    const String serializedVertices = parameterValue(parametersToUse, Effect2DMeshState::parameterId());
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
        const double delta = (1. - kIrPadding) / (double) std::max<size_t>(1, impulse.size() - 1);
        Buffer<float> oversampledBuffer(
                oversampledImpulse.data(),
                (int) oversampledImpulse.size());
        impulseOversampler.setMemoryBuffer(oversampledBuffer);
        (void) rasterizer.sampler().samplePerfectly(
                delta / impulseOversampler.getOversampleFactor(),
                oversampledBuffer,
                kIrPadding);
        impulseOversampler.sampleDown(oversampledBuffer, rawImpulseBuffer);
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
            { impulse.data(), (int) impulse.size() });
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

void DelaySignalProcessor::prepareProcess(
        const std::vector<NodeParameter>& parametersToUse,
        const AudioProcessTiming& timing) {
    bpm = std::max(1.0, timing.bpm);
    beatsPerMeasure = std::max(1, timing.beatsPerMeasure);

    CycleDsp::DelayConfiguration configuration;
    configuration.sampleRate = std::max(1.0, timing.sampleRate);
    configuration.delaySeconds = CycleDsp::delayTimeSeconds(
            parameterFloat(parametersToUse, "time", 0.5f),
            bpm,
            beatsPerMeasure);
    configuration.feedback = jlimit(
            0.f,
            0.98f,
            parameterFloat(parametersToUse, "feedback", 0.5f));
    configuration.spin = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "spin", 1.f));
    configuration.wet = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "wet", 0.9f));
    configuration.spinIterations = CycleDsp::delaySpinIterations(
            parameterFloat(parametersToUse, "spinIters", 0.f));

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

void ReverbSignalProcessor::prepareProcess(
        const std::vector<NodeParameter>& parametersToUse,
        const AudioProcessTiming&) {
    roomSize = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "size", 0.35f));
    rolloffFactor = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "damp", 0.2f)) * 0.7f;
    width = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "width", 0.75f));
    highPass = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "highPass", 0.05f));
    wetLevel = 0.25f * jlimit(0.f, 1.f, parameterFloat(parametersToUse, "wet", 0.5f));
    kernelLength = (size_t) NumberUtils::nextPower2((unsigned) std::pow(2.f, 12.f + 6.f * roomSize));
    syncKernel();
}

void ReverbSignalProcessor::beginBlock(size_t frameCount) {
    activeConvolver = &blockConvolver;
    prepareConvolver(blockConvolver, frameCount, preparedBlockSize);
    blockKernelRevision = kernelRevision;
}

void ReverbSignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    activeConvolver = &traversalConvolver;
    preparedTraversalSize = 0;
    prepareConvolver(traversalConvolver, rows, preparedTraversalSize);
    traversalKernelRevision = kernelRevision;
}

void ReverbSignalProcessor::endTraversalGrid() {
    activeConvolver = &blockConvolver;
}

void ReverbSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (buffer.empty() || kernel.empty() || activeConvolver == nullptr) {
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
            { kernel.data(), (int) kernel.size() });
    preparedSize = blockSize;
    dryBuffer.resize(blockSize);
    convolutionOutput.resize(blockSize);
}

}
