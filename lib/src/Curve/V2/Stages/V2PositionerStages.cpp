#include "V2PositionerStages.h"

#include <algorithm>

#include "../../../Util/NumberUtils.h"

namespace {
constexpr float kMinDx = 0.0001f;

float applyScaling(float y, V2ScalingType scaling) {
    switch (scaling) {
        case V2ScalingType::Unipolar: {
            NumberUtils::constrain(y, 0.0f, 1.0f);
            return y;
        }
        case V2ScalingType::Bipolar: {
            float scaled = 2.0f * y - 1.0f;
            NumberUtils::constrain(scaled, -1.0f, 1.0f);
            return scaled;
        }
        case V2ScalingType::HalfBipolar: {
            float scaled = y - 0.5f;
            NumberUtils::constrain(scaled, -0.5f, 0.5f);
            return scaled;
        }
        default:
            return y;
    }
}

float wrapToRange(float x, float minX, float maxX) {
    float range = maxX - minX;
    if (range <= 0.0f) {
        return minX;
    }

    while (x >= maxX) {
        x -= range;
    }

    while (x < minX) {
        x += range;
    }

    return x;
}

void enforceStrictOrder(std::vector<Intercept>& intercepts, int count, float minX, float maxX) {
    if (count <= 1) {
        return;
    }

    for (int i = 1; i < count; ++i) {
        if (intercepts[i].x <= intercepts[i - 1].x) {
            intercepts[i].x = intercepts[i - 1].x + kMinDx;
        }
    }

    if (intercepts[count - 1].x > maxX) {
        intercepts[count - 1].x = maxX;
        for (int i = count - 2; i >= 0; --i) {
            float allowed = intercepts[i + 1].x - kMinDx;
            if (intercepts[i].x >= allowed) {
                intercepts[i].x = allowed;
            }
        }
    }

    if (intercepts[0].x < minX) {
        intercepts[0].x = minX;
        for (int i = 1; i < count; ++i) {
            float allowed = intercepts[i - 1].x + kMinDx;
            if (intercepts[i].x <= allowed) {
                intercepts[i].x = allowed;
            }
        }
    }
}

bool runPositioningStage(
    std::vector<Intercept>& ioIntercepts,
    int& ioCount,
    const V2PositionerContext& context,
    bool wraps) {
    if (ioCount < 0) {
        ioCount = 0;
    }

    if (ioCount > static_cast<int>(ioIntercepts.size())) {
        ioCount = static_cast<int>(ioIntercepts.size());
    }

    if (ioCount == 0) {
        return false;
    }

    for (int i = 0; i < ioCount; ++i) {
        Intercept& intercept = ioIntercepts[i];
        intercept.adjustedX = intercept.x;

        if (wraps) {
            intercept.adjustedX = wrapToRange(intercept.adjustedX, context.minX, context.maxX);
        } else {
            NumberUtils::constrain(intercept.adjustedX, context.minX, context.maxX);
        }

        intercept.x = intercept.adjustedX;
        intercept.y = applyScaling(intercept.y, context.scaling);
    }

    std::sort(ioIntercepts.begin(), ioIntercepts.begin() + ioCount);
    enforceStrictOrder(ioIntercepts, ioCount, context.minX, context.maxX);

    return true;
}
}

bool V2LinearPositionerStage::run(
    std::vector<Intercept>& ioIntercepts,
    int& ioCount,
    const V2PositionerContext& context) noexcept {
    return runPositioningStage(ioIntercepts, ioCount, context, false);
}

bool V2CyclicPositionerStage::run(
    std::vector<Intercept>& ioIntercepts,
    int& ioCount,
    const V2PositionerContext& context) noexcept {
    return runPositioningStage(ioIntercepts, ioCount, context, true);
}
