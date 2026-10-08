#pragma once

#include <cstdint>
#include <vector>

namespace CycleV2 {

class GuideCurveSnapshotProvider;

class TrimeshGuideRailRenderService {
public:
    static std::vector<std::vector<float>> prepareTables(
            GuideCurveSnapshotProvider& provider,
            uint32_t visualizationSeed);
};

}
