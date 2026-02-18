#include "V2CurveBuilderStages.h"

#include <algorithm>

namespace {
void appendGenericCurvePadding(
    const std::vector<Intercept>& intercepts,
    int count,
    std::vector<Curve>& outCurves) {
    float minX = intercepts[0].x;
    float maxX = intercepts[0].x;

    for (int i = 1; i < count; ++i) {
        minX = jmin(minX, intercepts[i].x);
        maxX = jmax(maxX, intercepts[i].x);
    }

    minX -= 0.25f;
    maxX += 0.25f;

    Intercept front1(minX - 0.08f, intercepts[0].y);
    Intercept front2(minX - 0.16f, intercepts[0].y);
    Intercept front3(minX - 0.24f, intercepts[0].y);
    Intercept back1(maxX + 0.08f, intercepts[count - 1].y);
    Intercept back2(maxX + 0.16f, intercepts[count - 1].y);
    Intercept back3(maxX + 0.24f, intercepts[count - 1].y);

    outCurves.emplace_back(front3, front2, front1);
    outCurves.emplace_back(front2, front1, intercepts[0]);
    outCurves.emplace_back(front1, intercepts[0], intercepts[1]);

    for (int i = 0; i < count - 2; ++i) {
        outCurves.emplace_back(intercepts[i], intercepts[i + 1], intercepts[i + 2]);
    }

    outCurves.emplace_back(intercepts[count - 2], intercepts[count - 1], back1);
    outCurves.emplace_back(intercepts[count - 1], back1, back2);
    outCurves.emplace_back(back1, back2, back3);
}

void appendFxLegacyCurvePadding(
    const std::vector<Intercept>& intercepts,
    int count,
    std::vector<Curve>& outCurves) {
    Intercept front1(-1.0f, intercepts[0].y);
    Intercept front2(-0.5f, intercepts[0].y);
    Intercept back1(1.5f, intercepts[count - 1].y);
    Intercept back2(2.0f, intercepts[count - 1].y);

    outCurves.emplace_back(front1, front2, intercepts[0]);
    outCurves.emplace_back(front2, intercepts[0], intercepts[1]);

    for (int i = 0; i < count - 2; ++i) {
        outCurves.emplace_back(intercepts[i], intercepts[i + 1], intercepts[i + 2]);
    }

    outCurves.emplace_back(intercepts[count - 2], intercepts[count - 1], back1);
    outCurves.emplace_back(intercepts[count - 1], back1, back2);
}

void applyResolutionPolicy(std::vector<Curve>& curves, const V2CurveBuilderContext& context) {
    if (curves.size() < 3) {
        return;
    }

    if (context.lowResolution && curves.size() > 8) {
        for (auto& curve : curves) {
            curve.resIndex = Curve::resolutions - 1;
            curve.setShouldInterpolate(false);
        }
        return;
    }

    for (auto& curve : curves) {
        curve.setShouldInterpolate(! context.lowResolution && context.interpolateCurves);
    }

    float baseFactor = context.lowResolution ? 0.4f : context.integralSampling ? 0.05f : 0.1f;
    float base = baseFactor / static_cast<float>(Curve::resolution);

    for (int i = 1; i < static_cast<int>(curves.size()) - 1; ++i) {
        float dx = curves[i + 1].c.x - curves[i - 1].a.x;

        for (int j = 0; j < Curve::resolutions; ++j) {
            int res = Curve::resolution >> j;
            if (dx < base * static_cast<float>(res)) {
                curves[i].resIndex = j;
            }
        }
    }

    const int padding = 2;
    int lastIdx = static_cast<int>(curves.size()) - 1;
    curves.front().resIndex = curves[lastIdx - 2 * (padding - 1)].resIndex;
    curves.back().resIndex = curves[2 * padding - 1].resIndex;
}
}

bool V2DefaultCurveBuilderStage::run(
    const std::vector<Intercept>& inIntercepts,
    int inCount,
    std::vector<Curve>& outCurves,
    int& outCount,
    const V2CurveBuilderContext& context) noexcept {
    outCurves.clear();
    outCount = 0;

    if (inCount < 2 || inCount > static_cast<int>(inIntercepts.size())) {
        return false;
    }

    if (Curve::table == nullptr) {
        Curve::calcTable();
    }

    std::vector<Intercept> localIntercepts(inIntercepts.begin(), inIntercepts.begin() + inCount);
    outCurves.reserve(static_cast<size_t>(inCount + 6));

    if (context.paddingPolicy == V2CurveBuilderContext::PaddingPolicy::FxLegacyFixed) {
        appendFxLegacyCurvePadding(localIntercepts, inCount, outCurves);
    } else {
        appendGenericCurvePadding(localIntercepts, inCount, outCurves);
    }

    applyResolutionPolicy(outCurves, context);

    for (auto& curve : outCurves) {
        curve.recalculateCurve();
    }

    outCount = static_cast<int>(outCurves.size());
    return outCount > 0;
}
