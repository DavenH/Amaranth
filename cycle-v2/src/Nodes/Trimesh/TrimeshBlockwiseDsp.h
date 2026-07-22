#pragma once

#include "../../Runtime/NodeAudioProcessor.h"

#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>
#include <Obj/MorphPosition.h>

class Mesh;

namespace CycleV2 {

struct TrimeshConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::MeshSource; }

    std::shared_ptr<const Mesh> mesh;
    MorphPosition morph { 0.5f, 0.5f, 0.5f };
    int primaryViewAxis { Vertex::Time };
};

class TrimeshBlockwiseDsp {
public:
    void prepare(
            Mesh* meshToRender,
            const MorphPosition& morphPosition,
            int axis,
            bool shouldWrap);
    void setMesh(Mesh* meshToRender);
    void setMorphPosition(const MorphPosition& morphPosition);
    void setPrimaryViewAxis(int axis);
    void setCyclic(bool shouldWrap);

    void renderCycle(
            size_t frameCount,
            PortDomain domain,
            ChannelLayout channelLayout,
            SignalPayload& output);
    void renderPrepared(
            size_t frameCount,
            PortDomain domain,
            ChannelLayout channelLayout,
            SignalPayload& output);
    void renderCycleInto(Buffer<float> output);
    void renderPreparedInto(Buffer<float> output);

private:
    Rasterization::RasterizationRequest createRequest() const;
    void sampleOutput(Buffer<float> output);
    Buffer<float> outputBuffer(SignalPayload& output) const;

    bool cyclic { true };
    int primaryViewAxis { Vertex::Time };
    Mesh* mesh {};
    MorphPosition morph { 0.5f, 0.5f, 0.5f };
    Rasterization::TrilinearMeshRasterizer rasterizer;
};

}
