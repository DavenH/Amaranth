#pragma once

#include "../../Runtime/UnarySignalProcessor.h"
#include "../../Runtime/NodeDspConfiguration.h"

#include <Algo/Oversampler.h>
#include <Audio/WaveshaperTransfer.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>
#include <Inter/Dimensions.h>

namespace CycleV2 {

struct WaveshaperConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::Waveshaper; }

    std::shared_ptr<const WaveshaperTransfer> transfer;
    float preGain { 1.f };
    float postGain { 1.f };
    int oversampleFactor { 1 };
};

class WaveshaperSignalProcessor :
        public IUnarySignalOperation {
public:
    WaveshaperSignalProcessor();
    ~WaveshaperSignalProcessor();

    static std::shared_ptr<const WaveshaperConfiguration> buildConfiguration(
            const std::vector<NodeParameter>& parameters);

    void prepareExecution(const AudioExecutionSpec& spec);
    void adoptConfiguration(const PublishedNodeConfiguration& published);

    void prepareLegacy(
            const std::vector<NodeParameter>& parameters,
            const AudioProcessTiming& timing);
    void beginBlock(size_t frameCount) override;
    void beginTraversalGrid(size_t columns, size_t rows) override;
    void endTraversalGrid() override;
    void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) override;

private:
    void syncTransferTable(const std::vector<NodeParameter>& parameters);
    void rebuildMesh(const String& serializedVertices);
    void addVertex(float x, float y, float curve);

    Mesh mesh { "CycleV2WaveshaperDspMesh" };
    FXRasterizer rasterizer { nullptr, "CycleV2WaveshaperDspRasterizer" };
    WaveshaperTransfer transfer;
    Oversampler oversampler { 16 };
    std::vector<float> oversampleMemory;
    String lastVertexState;
    float preGain { 1.f };
    float postGain { 1.f };
    int oversampleFactor { 1 };
    bool useOversampling {};
    uint64_t adoptedRevision {};
    std::shared_ptr<const WaveshaperConfiguration> configuration;
};

}
