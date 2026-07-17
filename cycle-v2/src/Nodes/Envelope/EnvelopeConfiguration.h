#pragma once

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

#include "../../Runtime/NodeDspConfiguration.h"

namespace CycleV2 {

struct EnvelopeConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::Envelope; }

    std::shared_ptr<EnvelopeMesh> mesh;
    std::shared_ptr<EnvRasterizer> rasterizer;
    float level { 1.f };
    float redMorph { 0.5f };
    float blueMorph { 0.5f };
    bool logarithmic {};
    bool dynamicWhileLive {};
    String meshSnapshot;
};

}
