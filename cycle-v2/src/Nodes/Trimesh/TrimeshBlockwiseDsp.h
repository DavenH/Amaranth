#pragma once

#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>
#include <Obj/MorphPosition.h>

#include <vector>

#include "../../Runtime/NodeAudioProcessor.h"
#include "../Guide/GuideCurveSnapshotProvider.h"

class Mesh;

namespace CycleV2 {

struct TrimeshConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return processorRole; }

    AudioModuleRole processorRole { AudioModuleRole::MeshSource };
    std::shared_ptr<const Mesh> mesh;
    MorphPosition morph { 0.5f, 0.5f, 0.5f };
    int primaryViewAxis { Vertex::Time };
    float gain { 1.f };
    std::shared_ptr<GuideCurveSnapshotProvider> guideCurveProvider;
    size_t guideAssignmentCount {};
};

class TrimeshBlockwiseDsp {
public:
    void prepare(
            Mesh* meshToRender,
            const MorphPosition& morphPosition,
            int axis,
            bool shouldWrap,
            PortDomain domain);
    void prepareSampling(size_t maximumFrameCount);
    void setMesh(Mesh* meshToRender);
    void setMorphPosition(const MorphPosition& morphPosition);
    void setPrimaryViewAxis(int axis);
    void setCyclic(bool shouldWrap);
    void setGuideCurveProvider(GuideCurveProvider* provider);
    void setFrequencyMidiNote(int midiNote);

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
    void renderCycleInto(Buffer<float> output, PortDomain domain);
    void renderPreparedInto(Buffer<float> output);

private:
    void configureGuideCurveSeeds(PortDomain domain);
    Rasterization::RasterizationRequest createRequest(PortDomain domain) const;
    void sampleOutput(Buffer<float> output);
    Buffer<float> frequencyPositionsFor(int size);
    Buffer<float> outputBuffer(SignalPayload& output) const;

    bool cyclic { true };
    int frequencyMidiNote { 48 };
    int cachedFrequencyMidiNote { -1 };
    int cachedFrequencyPositionCount {};
    int primaryViewAxis { Vertex::Time };
    PortDomain preparedDomain { PortDomain::TimeSignal };
    MorphPosition morph { 0.5f, 0.5f, 0.5f };
    std::vector<float> frequencyPositions;
    Rasterization::TrilinearMeshRasterizer rasterizer;
    GuideCurveProvider* guideCurveProvider {};
    Mesh* mesh {};
};

}
