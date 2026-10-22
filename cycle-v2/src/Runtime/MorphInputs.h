#pragma once

#include <algorithm>
#include <array>
#include <Curve/Mesh/MorphDimensions.h>
#include "Runtime/AudioProcessTypes.h"

namespace CycleV2 {

struct MorphInputs {
    std::array<const SignalPayload*, 3> absoluteMorph {};
    std::array<float, 3> absoluteOverrides {};
    std::array<bool, 3> hasAbsoluteOverride {};
    const SignalPayload* scratch {};
    float scratchOverride {};
    bool hasScratchOverride {};

    static float value(const SignalPayload* input, size_t offset, float fallback) {
        if (input == nullptr || input->block.samples.empty()) {
            return fallback;
        }
        return jlimit(0.f, 1.f, input->block.samples[
                std::min(offset, input->block.samples.size() - 1)]);
    }

    float value(size_t axis, size_t offset, float fallback) const {
        return hasAbsoluteOverride[axis] ? jlimit(0.f, 1.f, absoluteOverrides[axis])
                : value(absoluteMorph[axis], offset, fallback);
    }

    MorphPosition resolve(const MorphPosition& fallback, MorphDimensions dimensions, size_t offset) const {
        MorphPosition result = fallback;
        for (size_t index = 0; index < MorphDimensions::axes.size(); ++index) {
            const int axis = MorphDimensions::axes[index];
            if (dimensions.active(axis)) {
                result[axis].setValueDirect(value(index, offset, fallback[axis]));
            }
        }
        return result;
    }
};

using TrimeshMorphInputs = MorphInputs;

}
