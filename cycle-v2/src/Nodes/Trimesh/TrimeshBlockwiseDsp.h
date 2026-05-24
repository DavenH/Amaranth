#pragma once

#include "../../Runtime/NodeAudioProcessor.h"

#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>
#include <Obj/MorphPosition.h>

class Mesh;

namespace CycleV2 {

class TrimeshBlockwiseDsp {
public:
    void setMesh(Mesh* meshToRender);
    void setMorphPosition(const MorphPosition& morphPosition);
    void setPrimaryViewAxis(int axis);
    void setCyclic(bool shouldWrap);

    void renderCycle(
            size_t frameCount,
            PortDomain domain,
            ChannelLayout channelLayout,
            AudioProcessBlock& output);

private:
    void prepareRequest();
    void sampleOutput(AudioProcessBlock& output);
    Buffer<float> outputBuffer(AudioProcessBlock& output) const;

    bool cyclic { true };
    int primaryViewAxis { Vertex::Time };
    Mesh* mesh {};
    MorphPosition morph { 0.5f, 0.5f, 0.5f };
    Rasterization::TrilinearMeshRasterizer rasterizer;
};

}
