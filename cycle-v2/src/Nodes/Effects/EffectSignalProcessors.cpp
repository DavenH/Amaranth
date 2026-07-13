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

size_t impulseLengthFor(float value, size_t minimumLength = 32) {
    const int exponent = jlimit(5, 12, 5 + (int) (value * 7.f));
    return std::max(minimumLength, (size_t) 1 << exponent);
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
}

IrSignalProcessor::~IrSignalProcessor() {
    mesh.destroy();
}

void IrSignalProcessor::prepareProcess(
        const std::vector<NodeParameter>& parametersToUse,
        const AudioProcessTiming&) {
    postGain = parameterFloat(parametersToUse, "post", 0.5f) * 2.f;
    highPass = parameterFloat(parametersToUse, "highPass", 0.5f);
    impulseLength = impulseLengthFor(parameterFloat(parametersToUse, "size", 0.5f));
    syncImpulse(parametersToUse);
}

void IrSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (buffer.empty() || impulse.empty()) {
        return;
    }

    convolutionOutput.resize(buffer.size() + impulse.size() - 1);
    ConvReverb::basicConvolve(
            buffer,
            { impulse.data(), (int) impulse.size() },
            { convolutionOutput.data(), (int) convolutionOutput.size() });

    VecOps::copy(convolutionOutput.data(), buffer.get(), buffer.size());

    if (highPass > 0.f && buffer.size() > 1) {
        float previous = buffer.front();
        for (int i = 1; i < buffer.size(); ++i) {
            const float current = buffer[i];
            buffer[i] = current - previous * highPass;
            previous = current;
        }
    }

    buffer.mul(postGain);
}

void IrSignalProcessor::syncImpulse(const std::vector<NodeParameter>& parametersToUse) {
    const String serializedVertices = parameterValue(parametersToUse, Effect2DMeshState::parameterId());
    if (serializedVertices == lastVertexState && impulse.size() == impulseLength) {
        return;
    }

    rebuildMesh(serializedVertices);
    lastVertexState = serializedVertices;
    ensureCurveTable();

    impulse.resize(impulseLength);
    rasterizer.updateGeometry();
    rasterizer.updateWaveform();

    Buffer<float> impulseBuffer(impulse.data(), (int) impulse.size());
    if (rasterizer.canRasterizeWaveform()) {
        const float delta = (1.f - kIrPadding) / (float) std::max<size_t>(1, impulse.size() - 1);
        (void) rasterizer.sampler().sampleWithInterval(impulseBuffer, delta, kIrPadding);
    } else {
        impulseBuffer.zero();
        impulseBuffer.front() = 1.f;
    }
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

void ReverbSignalProcessor::beginBlock(size_t) {
    activeCarry = &blockCarry;
}

void ReverbSignalProcessor::beginTraversalGrid(size_t, size_t) {
    activeCarry = &gridCarry;
    resetCarry();
}

void ReverbSignalProcessor::endTraversalGrid() {
    activeCarry = &blockCarry;
}

void ReverbSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (buffer.empty() || kernel.empty() || activeCarry == nullptr) {
        return;
    }

    std::vector<float> dry((size_t) buffer.size());
    VecOps::copy(buffer.get(), dry.data(), buffer.size());

    convolutionOutput.assign((size_t) buffer.size() + kernel.size() - 1, 0.f);
    ConvReverb::basicConvolve(
            buffer,
            { kernel.data(), (int) kernel.size() },
            { convolutionOutput.data(), (int) convolutionOutput.size() });

    auto& carry = *activeCarry;
    const size_t carryCount = std::min(carry.size(), (size_t) buffer.size());
    if (carryCount > 0) {
        Buffer<float> carryBuffer(carry.data(), (int) carryCount);
        Buffer<float> outputHead(convolutionOutput.data(), (int) carryCount);
        outputHead.add(carryBuffer);
    }

    const float dryScale = 1.f - 0.25f * wetLevel;
    Buffer<float> dryBuffer(dry.data(), buffer.size());
    buffer.mul(0.f);
    buffer.addProduct(dryBuffer, dryScale);
    buffer.addProduct({ convolutionOutput.data(), buffer.size() }, wetLevel);

    const size_t tailSize = convolutionOutput.size() - (size_t) buffer.size();
    carry.resize(tailSize);
    if (tailSize > 0) {
        VecOps::copy(convolutionOutput.data() + buffer.size(), carry.data(), (int) tailSize);
    }
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
    blockCarry.clear();
    gridCarry.clear();

    CycleDsp::ReverbKernelConfiguration configuration;
    configuration.roomSize = roomSize;
    configuration.damping = rolloffFactor;
    configuration.highPass = highPass;
    CycleDsp::buildReverbKernel(
            configuration,
            { kernel.data(), (int) kernel.size() });
}

void ReverbSignalProcessor::resetCarry() {
    if (activeCarry != nullptr) {
        activeCarry->clear();
    }
}

}
