#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace CycleV2 {

struct EnvelopeConfiguration;

class EnvelopePitchPreview {
public:
    static std::vector<std::vector<float>> renderLanes(
            const std::shared_ptr<const EnvelopeConfiguration>& configuration,
            int laneCount,
            int sampleCount,
            uint32_t seed);
};

}
