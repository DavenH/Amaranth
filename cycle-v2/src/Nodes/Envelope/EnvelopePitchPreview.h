#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace CycleV2 {

struct EnvelopeConfiguration;

class EnvelopePitchPreview {
public:
    static constexpr int defaultSampleCount = 129;
    static constexpr uint32_t unisonPreviewSeed = 0x554e4953u;

    static std::vector<std::vector<float>> renderLanes(
            const std::shared_ptr<const EnvelopeConfiguration>& configuration,
            int laneCount,
            int sampleCount,
            uint32_t seed,
            double releaseDuration = 0.,
            double noteOffTime = 1.);
};

}
