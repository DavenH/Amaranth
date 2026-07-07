#include "EffectSignalProcessors.h"

#include "../Effect2D/Effect2DMeshState.h"

#include <Algo/ConvReverb.h>
#include <Algo/FFT.h>
#include <Array/ScopedAlloc.h>
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
    sampleRate = (float) std::max(1.0, timing.sampleRate);
    bpm = std::max(1.0, timing.bpm);
    beatsPerMeasure = std::max(1, timing.beatsPerMeasure);
    delayTimeSeconds = cycle1DelayTimeSeconds(parameterFloat(parametersToUse, "time", 0.5f));
    feedback = jlimit(0.f, 0.98f, parameterFloat(parametersToUse, "feedback", 0.5f));
    spinAmount = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "spin", 1.f));
    wetLevel = jlimit(0.f, 1.f, parameterFloat(parametersToUse, "wet", 0.9f));
    spinIters = cycle1SpinIters(parameterFloat(parametersToUse, "spinIters", 0.f));
    configureDelayLines();
}

void DelaySignalProcessor::beginBlock(size_t) {
    activeState = &blockState;
}

void DelaySignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    (void) rows;
    activeState = &gridState;
    resetDelayState(gridState);
}

void DelaySignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (buffer.empty()
            || activeState == nullptr
            || activeState->inputBuffer.empty()
            || activeState->spinParams.empty()) {
        return;
    }

    auto& state = *activeState;
    const size_t inputMask = state.inputBuffer.size() - 1;
    const size_t spinMask = state.spinParams.front().buffer.size() - 1;
    const float pownFeedback = std::pow(feedback, (int) state.spinParams.size()) + 1e-17f;

    for (int i = 0; i < buffer.size(); ++i) {
        const float input = buffer[i];
        float wetSum = 0.f;
        const size_t readPos = state.readPosition & spinMask;

        state.inputBuffer[state.readPosition & inputMask] = input;

        for (auto& params : state.spinParams) {
            const size_t bufferReadPos = (state.readPosition - params.inputDelaySamples) & inputMask;
            const size_t selfReadPos = (state.readPosition - params.spinDelaySamples) & spinMask;
            const float spinDelayedSelf = pownFeedback * params.buffer[selfReadPos] + 1e-17f;
            params.buffer[readPos] = state.inputBuffer[bufferReadPos] + spinDelayedSelf;
            wetSum += params.pan * params.startingLevel * params.buffer[readPos];
        }

        buffer[i] = input + wetLevel * wetSum + 1e-19f;
        state.readPosition++;
    }
}

void DelaySignalProcessor::configureDelayLines() {
    const String signature = String(delayTimeSeconds)
            + ":" + String(feedback)
            + ":" + String(spinAmount)
            + ":" + String(sampleRate)
            + ":" + String(spinIters);

    if (signature == delaySignature) {
        return;
    }

    delaySignature = signature;
    configureDelayState(blockState);
    configureDelayState(gridState);
}

void DelaySignalProcessor::configureDelayState(DelayState& state) {
    const int singleSize = std::max(1, (int) (spinIters * delayTimeSeconds * sampleRate));
    const size_t bufferSize = std::max<size_t>(2, (size_t) NumberUtils::nextPower2((unsigned) singleSize));

    state.inputBuffer.assign(bufferSize, 0.f);
    state.spinParams.resize((size_t) spinIters);
    state.readPosition = 0;

    for (int spinIdx = 0; spinIdx < spinIters; ++spinIdx) {
        auto& params = state.spinParams[(size_t) spinIdx];
        const float pan = 0.5f + 0.49999f
                * spinAmount
                * std::sin(spinIdx / (float) spinIters * MathConstants<float>::twoPi);
        params.pan = std::min(1.f, 2.f * (1.f - pan));
        params.startingLevel = std::pow(feedback, spinIdx + 1);
        params.inputDelaySamples = (size_t) std::max<int>(
                1,
                (int) ((spinIdx + 1) * delayTimeSeconds * sampleRate));
        params.spinDelaySamples = (size_t) std::max<int>(
                1,
                (int) (spinIters * delayTimeSeconds * sampleRate));
        params.buffer.assign(bufferSize, 0.f);
    }
}

void DelaySignalProcessor::resetDelayState(DelayState& state) {
    std::fill(state.inputBuffer.begin(), state.inputBuffer.end(), 0.f);
    for (auto& params : state.spinParams) {
        std::fill(params.buffer.begin(), params.buffer.end(), 0.f);
    }
    state.readPosition = 0;
}

double DelaySignalProcessor::cycle1DelayTimeSeconds(float value) const {
    const double unitValue = std::max(0.15f, value);
    const double secondsPerBeat = 60.0 / bpm;
    return beatsPerMeasure * unitValue * unitValue * secondsPerBeat;
}

int DelaySignalProcessor::cycle1SpinIters(float value) const {
    return std::max(1, (int) (12.f * value * value));
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

    constexpr int bufferSize = 256;
    if (kernelLength < bufferSize) {
        return;
    }

    Transform fft;
    fft.allocate(bufferSize, Transform::DivFwdByN, true);

    ScopedAlloc<float> memory(bufferSize * 10);
    Buffer<float> cumulativeRolloff = memory.place(bufferSize);
    Buffer<float> magnitude = memory.place(bufferSize);
    Buffer<float> phase = memory.place(bufferSize);
    Buffer<float> noise = memory.place(bufferSize);
    Buffer<float> rolloff = memory.place(bufferSize);
    Buffer<float> volumeRamp = memory.place(bufferSize);
    Buffer<float> filtered = memory.place(bufferSize);
    Buffer<float> filteredA = memory.place(bufferSize);
    Buffer<float> forwardRamp = memory.place(bufferSize);
    Buffer<float> inverseRamp = memory.place(bufferSize);
    (void) magnitude;
    (void) phase;

    const int highStart = std::max(2, (int) (highPass * 0.05f * bufferSize));
    const float highLevelA = 1.f - rolloffFactor * highPass * 1.5f;
    const float highLevelRamp = (1.f - highLevelA) / (float) highStart;

    rolloff.zero();
    rolloff.ramp(1.f, -rolloffFactor / (float) bufferSize);
    forwardRamp.section(0, highStart).ramp(highLevelA, highLevelRamp);
    rolloff.section(0, highStart).mul(forwardRamp);
    rolloff.mul(MathConstants<float>::halfPi).sin();

    forwardRamp.ramp();
    forwardRamp.copyTo(inverseRamp);
    inverseRamp.subCRev(1.f);

    unsigned seed = bufferSize ^ 0xc0d3db4d;
    const int numBuffers = (int) kernelLength / bufferSize;
    cumulativeRolloff.set(1.f);

    for (int i = 0; i < numBuffers; ++i) {
        Buffer<float> section(kernel.data() + i * bufferSize, bufferSize);

        createVolumeRamp(i, numBuffers, volumeRamp);

        noise.rand(seed);
        fft.forward(noise);
        fft.getMagnitudes().mul(cumulativeRolloff);
        cumulativeRolloff.mul(rolloff);

        fft.inverse(filteredA);
        fft.getMagnitudes().mul(rolloff);
        fft.inverse(filtered);

        VecOps::mul(filteredA, inverseRamp, section);
        section.addProduct(forwardRamp, filtered);
        section.mul(volumeRamp);

        seed ^= 616137 * seed;
    }
}

void ReverbSignalProcessor::resetCarry() {
    if (activeCarry != nullptr) {
        activeCarry->clear();
    }
}

void ReverbSignalProcessor::createVolumeRamp(
        int index,
        int numBuffers,
        Buffer<float> ramp) const {
    const int bufferSize = ramp.size();
    const int rampUpBuffers = jlimit(3, 20, (int) (roomSize * 0.03f * numBuffers));
    float rampStart {};
    float rampEnd {};

    if (index < rampUpBuffers) {
        rampStart = 0.25f * (float) index / (float) (rampUpBuffers - 1);
        rampEnd = 0.25f * (float) (index + 1) / (float) (rampUpBuffers - 1);
    } else {
        const float scale = MathConstants<float>::halfPi
                * ((float) (NumberUtils::log2i((unsigned) numBuffers) + 8) * 0.1f - 0.6f);
        rampStart = 0.25f / scale
                * std::sin(scale * std::exp(index * -4.5f / (float) numBuffers))
                * std::sqrt((float) (numBuffers - index) / (float) numBuffers);
        rampEnd = 0.25f / scale
                * std::sin(scale * std::exp((index + 1) * -4.5f / (float) numBuffers))
                * std::sqrt((float) (numBuffers - (index + 1)) / (float) numBuffers);
    }

    ramp.ramp(rampStart, (rampEnd - rampStart) / (float) bufferSize);

    const int silence = (int) (roomSize * 450.f) - index * bufferSize;
    if (silence > 0) {
        ramp.zero(std::min(bufferSize, silence));
    }
}

}
