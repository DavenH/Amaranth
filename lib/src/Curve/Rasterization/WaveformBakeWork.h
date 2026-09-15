#pragma once

#include <cstdint>

namespace Rasterization {

struct WaveformBakeWork {
    uint64_t waveformSegments {};
    uint64_t integralSegments {};
};

}
