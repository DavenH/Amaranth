#pragma once

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/EnvelopeMaterialization.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

#include "Nodes/Guide/GuideCurveSnapshotProvider.h"
#include "Runtime/NodeDspConfiguration.h"

namespace CycleV2 {

struct EnvelopeConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::Envelope; }

    std::shared_ptr<EnvelopeMesh> mesh;
    std::shared_ptr<GuideCurveSnapshotProvider> guideCurveProvider;
    std::shared_ptr<EnvRasterizer> rasterizer;
    Rasterization::RealtimeEnvelopePlan realtimePlan;
    Rasterization::RealtimeEnvelopePlan cycleRealtimePlan;
    float level { 1.f };
    std::shared_ptr<const Rasterization::PreparedMeshSlice> staticSlice;
    MorphPosition morph { 0.f, 0.5f, 0.5f };
    int dimensionCount { 3 };
    float neutralValue {};
    bool logarithmic {};
    bool enabled { true };
    bool lowResolution {};
    bool cycleGuideRequired {};
    bool volumePurpose {};
    bool declick {};
};

}
