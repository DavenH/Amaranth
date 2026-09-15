#pragma once

#include <Array/Buffer.h>

namespace CycleDsp {

class UnisonColumnMixer {
public:
    static bool mix(
            Buffer<float> source,
            Buffer<float> destination,
            const float* phaseCycles,
            const float* gains,
            int voiceCount,
            Buffer<float> shifted,
            Buffer<float> interpolated,
            bool interpolate);
};

}
