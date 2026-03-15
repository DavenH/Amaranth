#include "V2PositionerStages.h"

#include <algorithm>
#include <cmath>

#include <Curve/GuideCurveProvider.h>
#include <Util/NumberUtils.h>

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

bool sanitizeCount(std::vector<Intercept>& ioIntercepts, int& ioCount) {
    if (ioCount < 0) {
        ioCount = 0;
    }

    if (ioCount > static_cast<int>(ioIntercepts.size())) {
        ioCount = static_cast<int>(ioIntercepts.size());
    }

    return ioCount > 0;
}

float getTimeProgress(const Intercept& intercept, const V2PositionerContext& context) {
    if (intercept.cube == nullptr) {
        return 0.0f;
    }

    if (context.pointPath.useLegacyTimeProgress) {
        VertCube::ReductionData reduction;
        intercept.cube->getInterceptsFast(context.interpolationDimension, reduction, context.morph);

        float timeMin = reduction.v0.values[Vertex::Time];
        float timeMax = reduction.v1.values[Vertex::Time];
        if (timeMin > timeMax) {
            std::swap(timeMin, timeMax);
        }

        float diffTime = timeMax - timeMin;
        if (diffTime == 0.0f) {
            return 0.0f;
        }

        return std::fabs(reduction.v.values[Vertex::Time] - timeMin) / diffTime;
    }

    return intercept.cube->getPortionAlong(Vertex::Time, context.morph);
}

void wrapAdjustedX(Intercept& intercept, const V2PositionerContext& context) {
    float wrapped = wrapToRange(intercept.adjustedX, context.minX, context.maxX);
    if (wrapped != intercept.adjustedX) {
        intercept.isWrapped = true;
        intercept.adjustedX = wrapped;
    }
}
}

bool V2ClampOrWrapPositionerStage::run(
    std::vector<Intercept>& ioIntercepts,
    int& ioCount,
    const V2PositionerContext& context) noexcept {
    if (! sanitizeCount(ioIntercepts, ioCount)) {
        return false;
    }

    for (int i = 0; i < ioCount; ++i) {
        Intercept& intercept = ioIntercepts[i];
        intercept.adjustedX = intercept.x;
        intercept.isWrapped = false;

        if (wraps) {
            intercept.adjustedX = wrapToRange(intercept.adjustedX, context.minX, context.maxX);
        } else {
            NumberUtils::constrain(intercept.adjustedX, context.minX, context.maxX);
        }

        intercept.x = intercept.adjustedX;
    }

    return true;
}

bool V2ApplyScalingPositionerStage::run(
    std::vector<Intercept>& ioIntercepts,
    int& ioCount,
    const V2PositionerContext& context) noexcept {
    if (! sanitizeCount(ioIntercepts, ioCount)) {
        return false;
    }

    for (int i = 0; i < ioCount; ++i) {
        ioIntercepts[i].y = applyScaling(ioIntercepts[i].y, context.scaling);
    }

    return true;
}

bool V2SortAndOrderPositionerStage::run(
    std::vector<Intercept>& ioIntercepts,
    int& ioCount,
    const V2PositionerContext& context) noexcept {
    if (! sanitizeCount(ioIntercepts, ioCount)) {
        return false;
    }

    std::sort(ioIntercepts.begin(), ioIntercepts.begin() + ioCount);
    enforceStrictOrder(ioIntercepts, ioCount, context.minX, context.maxX);
    return true;
}

bool V2PointPathPositionerStage::run(
    std::vector<Intercept>& ioIntercepts,
    int& ioCount,
    const V2PositionerContext& context) noexcept {
    if (! sanitizeCount(ioIntercepts, ioCount)) {
        return false;
    }

    const V2PositionerContext::PointPathContext& pointPath = context.pointPath;
    if (! pointPath.enabled
            || pointPath.path == nullptr
            || pointPath.vertOffsetSeeds == nullptr
            || pointPath.phaseOffsetSeeds == nullptr) {
        return true;
    }

    GuideCurveProvider::NoiseContext noise;
    noise.noiseSeed = pointPath.noiseSeed;

    for (int i = 0; i < ioCount; ++i) {
        Intercept& intercept = ioIntercepts[i];
        VertCube* cube = intercept.cube;
        if (cube == nullptr) {
            continue;
        }

        float redProgress = cube->getPortionAlong(Vertex::Red, context.morph);
        float blueProgress = cube->getPortionAlong(Vertex::Blue, context.morph);
        float timeProgress = getTimeProgress(intercept, context);
        bool ignoreTimeEdge = pointPath.noOffsetAtEnds && (timeProgress == 0.0f || timeProgress == 1.0f);

        if (cube->guideCurveAt(Vertex::Red) >= 0) {
            int dfrm = cube->guideCurveAt(Vertex::Red);
            noise.vertOffset = pointPath.vertOffsetSeeds[dfrm];
            noise.phaseOffset = pointPath.phaseOffsetSeeds[dfrm];
            intercept.adjustedX += cube->guideCurveAbsGain(Vertex::Red)
                * pointPath.path->getTableValue(dfrm, redProgress, noise);
        }

        if (cube->guideCurveAt(Vertex::Blue) >= 0) {
            int dfrm = cube->guideCurveAt(Vertex::Blue);
            noise.vertOffset = pointPath.vertOffsetSeeds[dfrm];
            noise.phaseOffset = pointPath.phaseOffsetSeeds[dfrm];
            intercept.adjustedX += cube->guideCurveAbsGain(Vertex::Blue)
                * pointPath.path->getTableValue(dfrm, blueProgress, noise);
        }

        if (cube->guideCurveAt(Vertex::Amp) >= 0) {
            int dfrm = cube->guideCurveAt(Vertex::Amp);
            noise.vertOffset = pointPath.vertOffsetSeeds[dfrm];
            noise.phaseOffset = pointPath.phaseOffsetSeeds[dfrm];
            if (! ignoreTimeEdge) {
                intercept.y += cube->guideCurveAbsGain(Vertex::Amp)
                    * pointPath.path->getTableValue(dfrm, timeProgress, noise);
                NumberUtils::constrain(intercept.y, (context.scaling == V2ScalingType::Unipolar ? 0.0f : -1.0f), 1.0f);
            }
        }

        if (cube->guideCurveAt(Vertex::Phase) >= 0) {
            int dfrm = cube->guideCurveAt(Vertex::Phase);
            noise.vertOffset = pointPath.vertOffsetSeeds[dfrm];
            noise.phaseOffset = pointPath.phaseOffsetSeeds[dfrm];
            if (! ignoreTimeEdge) {
                intercept.adjustedX += cube->guideCurveAbsGain(Vertex::Phase)
                    * pointPath.path->getTableValue(dfrm, timeProgress, noise);

                if (context.cyclic) {
                    wrapAdjustedX(intercept, context);
                }
            }
        }

        if (cube->guideCurveAt(Vertex::Curve) >= 0) {
            int dfrm = cube->guideCurveAt(Vertex::Curve);
            noise.vertOffset = pointPath.vertOffsetSeeds[dfrm];
            noise.phaseOffset = pointPath.phaseOffsetSeeds[dfrm];
            if (! ignoreTimeEdge) {
                intercept.shp += 2.0f * cube->guideCurveAbsGain(Vertex::Curve)
                    * pointPath.path->getTableValue(dfrm, timeProgress, noise);
                NumberUtils::constrain(intercept.shp, 0.0f, 1.0f);
            }
        }

        intercept.x = intercept.adjustedX;
    }

    return true;
}

void V2CompositePositionerStage::clear() noexcept {
    stageCount = 0;
}

bool V2CompositePositionerStage::addStage(V2PositionerStage* stage) noexcept {
    if (stage == nullptr || stageCount >= maxStages) {
        return false;
    }

    stages[stageCount++] = stage;
    return true;
}

void V2CompositePositionerStage::reset() noexcept {
    for (int i = 0; i < stageCount; ++i) {
        if (stages[i] != nullptr) {
            stages[i]->reset();
        }
    }
}

bool V2CompositePositionerStage::run(
    std::vector<Intercept>& ioIntercepts,
    int& ioCount,
    const V2PositionerContext& context) noexcept {
    if (stageCount == 0) {
        return false;
    }

    for (int i = 0; i < stageCount; ++i) {
        V2PositionerStage* stage = stages[i];
        if (stage == nullptr) {
            return false;
        }

        if (! stage->run(ioIntercepts, ioCount, context)) {
            return false;
        }
    }

    return ioCount > 0;
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

void V2VoiceChainingPositionerStage::reset() noexcept {
    previousIntercepts.clear();
    primed = false;
}

bool V2VoiceChainingPositionerStage::run(
    std::vector<Intercept>& ioIntercepts,
    int& ioCount,
    const V2PositionerContext& context) noexcept {
    if (! sanitizeCount(ioIntercepts, ioCount)) {
        return false;
    }

    previousIntercepts.assign(ioIntercepts.begin(), ioIntercepts.begin() + ioCount);
    primed = true;
    return true;
}
