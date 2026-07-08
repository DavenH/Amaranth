#pragma once

#include "../../Runtime/UnarySignalProcessor.h"

#include <Audio/WaveshaperTransfer.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>
#include <Inter/Dimensions.h>

namespace CycleV2 {

class WaveshaperSignalProcessor :
        public IUnarySignalOperation {
public:
    WaveshaperSignalProcessor();
    ~WaveshaperSignalProcessor();

    void prepareProcess(
            const std::vector<NodeParameter>& parameters,
            const AudioProcessTiming& timing) override;
    void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) override;

private:
    void syncTransferTable(const std::vector<NodeParameter>& parameters);
    void rebuildMesh(const String& serializedVertices);
    void addVertex(float x, float y, float curve);

    Mesh mesh { "CycleV2WaveshaperDspMesh" };
    FXRasterizer rasterizer { nullptr, "CycleV2WaveshaperDspRasterizer" };
    WaveshaperTransfer transfer;
    String lastVertexState;
    float preGain { 1.f };
    float postGain { 1.f };
};

}
