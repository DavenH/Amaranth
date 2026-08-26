#pragma once

#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Runtime/ChainedOscillatorRegionRuntime.h"

#include <Curve/Rasterization/Rasterizer/VoiceRasterizer.h>

#include <array>

namespace CycleV2 {

class TrimeshOscillatorCycleRenderer final : public OscillatorCycleRenderer {
public:
    bool prepare(
            std::shared_ptr<const TrimeshConfiguration> configurationToUse,
            int laneCount);
    void reset() override;
    void renderCycle(
            const ChainedCycleRenderRequest& request,
            Buffer<float> left,
            Buffer<float> right) override;

private:
    struct LaneRasterizer {
        Rasterization::VoiceCycleState state;
        Rasterization::VoiceRasterizer rasterizer;
        bool primed {};
    };

    int preparedLaneCount {};
    std::array<LaneRasterizer, CycleDsp::maximumUnisonOrder> lanes;
    std::shared_ptr<const TrimeshConfiguration> configuration;
};

}
