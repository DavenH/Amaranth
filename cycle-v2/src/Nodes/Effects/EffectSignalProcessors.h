#pragma once

#include "../../Runtime/UnarySignalProcessor.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>

namespace CycleV2 {

class IrSignalProcessor :
        public IUnarySignalOperation {
public:
    IrSignalProcessor();
    ~IrSignalProcessor();

    void prepareProcess(
            const std::vector<NodeParameter>& parameters,
            const AudioProcessTiming& timing) override;
    void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) override;

private:
    void syncImpulse(const std::vector<NodeParameter>& parameters);
    void rebuildMesh(const String& serializedVertices);
    void addVertex(float x, float y, float curve);

    Mesh mesh { "CycleV2IrDspMesh" };
    FXRasterizer rasterizer { nullptr, "CycleV2IrDspRasterizer" };
    String lastVertexState;
    std::vector<float> impulse;
    std::vector<float> convolutionOutput;
    float postGain { 1.f };
    float highPass { 0.f };
    size_t impulseLength { 1 };
};

class DelaySignalProcessor :
        public IUnarySignalOperation {
public:
    void prepareProcess(
            const std::vector<NodeParameter>& parameters,
            const AudioProcessTiming& timing) override;
    void beginBlock(size_t frameCount) override;
    void beginTraversalGrid(size_t columns, size_t rows) override;
    void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) override;

private:
    struct SpinParams {
        float pan {};
        float startingLevel {};
        size_t inputDelaySamples {};
        size_t spinDelaySamples {};
        std::vector<float> buffer;
    };

    struct DelayState {
        std::vector<float> inputBuffer;
        std::vector<SpinParams> spinParams;
        size_t readPosition {};
    };

    void configureDelayLines();
    void configureDelayState(DelayState& state);
    void resetDelayState(DelayState& state);
    double cycle1DelayTimeSeconds(float value) const;
    int cycle1SpinIters(float value) const;

    DelayState blockState;
    DelayState gridState;
    DelayState* activeState { &blockState };
    String delaySignature;
    double delayTimeSeconds { 0.5 };
    float feedback { 0.5f };
    float spinAmount { 1.f };
    float wetLevel { 0.9f };
    float sampleRate { 44100.f };
    double bpm { 120.0 };
    int beatsPerMeasure { 4 };
    int spinIters { 1 };
};

class ReverbSignalProcessor :
        public IUnarySignalOperation {
public:
    void prepareProcess(
            const std::vector<NodeParameter>& parameters,
            const AudioProcessTiming& timing) override;
    void beginBlock(size_t frameCount) override;
    void beginTraversalGrid(size_t columns, size_t rows) override;
    void endTraversalGrid() override;
    void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) override;

private:
    void syncKernel();
    void resetCarry();
    void createVolumeRamp(int index, int numBuffers, Buffer<float> ramp) const;

    std::vector<float> kernel;
    std::vector<float> convolutionOutput;
    std::vector<float> blockCarry;
    std::vector<float> gridCarry;
    std::vector<float>* activeCarry { &blockCarry };
    float roomSize { 0.2f };
    float rolloffFactor { 0.14f };
    float width { 1.f };
    float highPass { 0.05f };
    float wetLevel { 0.1f };
    size_t kernelLength {};
    String kernelSignature;
};

}
