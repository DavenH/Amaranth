#include "V2CurveBuilderStages.h"

#include <algorithm>
#include <climits>

namespace {
struct ResolutionPolicy {
    float baseFactor{0.1f};
    int lowResPolicyListSize{8};
};

void applyConfiguredResolutionPolicy(
    std::vector<Curve>& curves,
    const V2CurveBuilderContext& context,
    const ResolutionPolicy& policy) {
    if (curves.size() < 3) {
        return;
    }

    if (context.lowResolution
            && curves.size() > static_cast<size_t>(policy.lowResPolicyListSize)) {
        for (auto& curve : curves) {
            curve.resIndex = Curve::resolutions - 1;
            curve.setShouldInterpolate(false);
        }
        return;
    }

    for (auto& curve : curves) {
        curve.setShouldInterpolate(! context.lowResolution && context.interpolateCurves);
    }

    float base = policy.baseFactor / static_cast<float>(Curve::resolution);
    int lastIdx = static_cast<int>(curves.size()) - 1;

    for (int i = 1; i < lastIdx; ++i) {
        float dx = curves[i + 1].c.x - curves[i - 1].a.x;

        for (int j = 0; j < Curve::resolutions; ++j) {
            int res = Curve::resolution >> j;
            if (dx < base * static_cast<float>(res)) {
                curves[i].resIndex = j;
            }
        }
    }

    constexpr int padding = 2;
    curves.front().resIndex = curves[lastIdx - 2 * (padding - 1)].resIndex;
    curves.back().resIndex = curves[2 * padding - 1].resIndex;
}

void applyVoiceResolutionPolicy(std::vector<Curve>& curves, const V2CurveBuilderContext& context) {
    ResolutionPolicy policy;
    policy.baseFactor = 0.05f;
    policy.lowResPolicyListSize = INT_MAX;

    applyConfiguredResolutionPolicy(curves, context, policy);

    if (curves.size() < 3) {
        return;
    }

    int lastIdx = static_cast<int>(curves.size()) - 1;
    curves[0].resIndex = curves[1].resIndex;
    curves[lastIdx].resIndex = curves[lastIdx - 1].resIndex;
}

void applyGenericResolutionPolicy(std::vector<Curve>& curves, const V2CurveBuilderContext& context) {
    ResolutionPolicy policy;
    policy.baseFactor = context.lowResolution ? 0.4f : context.integralSampling ? 0.05f : 0.1f;
    applyConfiguredResolutionPolicy(curves, context, policy);
}

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

void appendVoiceChainingPadding(
    const std::vector<Intercept>& previousIntercepts,
    const std::vector<Intercept>& backIntercepts,
    int backCount,
    Intercept& frontA,
    Intercept& frontB,
    Intercept& frontC,
    Intercept& frontD,
    Intercept& frontE,
    std::vector<Curve>& outCurves) {
    const int end = static_cast<int>(previousIntercepts.size()) - 1;
    Intercept back1;
    Intercept back2;
    Intercept back3;
    Intercept back4;
    Intercept back5;

    back1.assignWithTranslation(backIntercepts[0], 1.0f);
    back2.assignWithTranslation(backIntercepts[1], 1.0f);
    back3.assignBack(const_cast<std::vector<Intercept>&>(backIntercepts), 3);
    back4.assignBack(const_cast<std::vector<Intercept>&>(backIntercepts), 4);
    back5.assignBack(const_cast<std::vector<Intercept>&>(backIntercepts), 5);

    bool extraPadFront = frontB.x > 0.0f;
    bool padFront = frontA.x > 0.0f;
    bool padBack = back1.x < 1.0f;
    bool extraPadBack = back2.x < 1.0f;

    if (extraPadFront) {
        outCurves.emplace_back(frontE, frontD, frontC);
    }

    if (padFront) {
        outCurves.emplace_back(frontD, frontC, frontB);
    }

    outCurves.emplace_back(frontC, frontB, frontA);
    outCurves.emplace_back(frontB, frontA, previousIntercepts[0]);
    outCurves.emplace_back(frontA, previousIntercepts[0], previousIntercepts[1]);

    for (int i = 0; i < end - 1; ++i) {
        outCurves.emplace_back(previousIntercepts[i], previousIntercepts[i + 1], previousIntercepts[i + 2]);
    }

    outCurves.emplace_back(previousIntercepts[end - 1], previousIntercepts[end], back1);
    outCurves.emplace_back(previousIntercepts[end], back1, back2);
    outCurves.emplace_back(back1, back2, back3);

    if (padBack) {
        outCurves.emplace_back(back2, back3, back4);
    }

    if (extraPadBack) {
        outCurves.emplace_back(back3, back4, back5);
    }

    frontE.assignFront(const_cast<std::vector<Intercept>&>(previousIntercepts), 5);
    frontD.assignFront(const_cast<std::vector<Intercept>&>(previousIntercepts), 4);
    frontC.assignFront(const_cast<std::vector<Intercept>&>(previousIntercepts), 3);
    frontB.assignWithTranslation(previousIntercepts[end - 1], -1.0f);
    frontA.assignWithTranslation(previousIntercepts[end], -1.0f);
}

}

void applyResolutionPolicy(std::vector<Curve>& curves, const V2CurveBuilderContext& context) {
    applyGenericResolutionPolicy(curves, context);
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

void V2VoiceChainingCurveBuilderStage::reset() noexcept {
    previousIntercepts.clear();
    frontA = Intercept(-0.05f, 0.0f);
    frontB = Intercept(-0.10f, 0.0f);
    frontC = Intercept(-0.15f, 0.0f);
    frontD = Intercept(-0.25f, 0.0f);
    frontE = Intercept(-0.35f, 0.0f);
}

void V2VoiceChainingCurveBuilderStage::prime(const std::vector<Intercept>& intercepts) noexcept {
    previousIntercepts.assign(intercepts.begin(), intercepts.end());
}

bool V2VoiceChainingCurveBuilderStage::run(
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

    std::vector<Intercept> backIntercepts(inIntercepts.begin(), inIntercepts.begin() + inCount);
    std::vector<Intercept> renderIntercepts = previousIntercepts.empty()
        ? backIntercepts
        : previousIntercepts;

    if (renderIntercepts.size() < 2 || backIntercepts.size() < 2) {
        previousIntercepts = backIntercepts;
        return false;
    }

    outCurves.reserve(static_cast<size_t>(renderIntercepts.size() + 9));
    appendVoiceChainingPadding(
        renderIntercepts,
        backIntercepts,
        inCount,
        frontA,
        frontB,
        frontC,
        frontD,
        frontE,
        outCurves);
    applyVoiceResolutionPolicy(outCurves, context);

    for (auto& curve : outCurves) {
        curve.recalculateCurve();
    }

    previousIntercepts = backIntercepts;
    outCount = static_cast<int>(outCurves.size());
    return outCount > 0;
}
