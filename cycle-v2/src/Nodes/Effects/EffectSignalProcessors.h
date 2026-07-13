#pragma once

#include "../../Runtime/UnarySignalProcessor.h"

#include <Algo/ConvReverb.h>
#include <Audio/CycleDsp/CycleDelay.h>
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
    CycleDsp::CycleDelay blockDelay;
    CycleDsp::CycleDelay traversalDelay;
    CycleDsp::CycleDelay* activeDelay { &blockDelay };
    double bpm { 120.0 };
    int beatsPerMeasure { 4 };
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
    void prepareConvolver(ConvReverb& convolver, size_t blockSize, size_t& preparedSize);

    std::vector<float> kernel;
    std::vector<float> convolutionOutput;
    std::vector<float> dryBuffer;
    ConvReverb blockConvolver;
    ConvReverb traversalConvolver;
    ConvReverb* activeConvolver { &blockConvolver };
    float roomSize { 0.2f };
    float rolloffFactor { 0.14f };
    float width { 1.f };
    float highPass { 0.05f };
    float wetLevel { 0.1f };
    size_t kernelLength {};
    size_t kernelRevision {};
    size_t blockKernelRevision {};
    size_t traversalKernelRevision {};
    size_t preparedBlockSize {};
    size_t preparedTraversalSize {};
    String kernelSignature;
};

}
