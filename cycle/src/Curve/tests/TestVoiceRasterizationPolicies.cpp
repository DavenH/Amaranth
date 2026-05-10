#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../CycleState.h"
#include "../Rasterization/Policies/Voice/VoicePolicies.h"

namespace {
    using Catch::Approx;

    std::vector<Intercept> makeCurrentIntercepts() {
        return {
            Intercept(0.10f, -0.50f, nullptr, 0.20f),
            Intercept(0.42f, 0.25f, nullptr, 0.35f),
            Intercept(0.78f, -0.10f, nullptr, 0.50f),
        };
    }

    std::vector<Intercept> makeNextIntercepts() {
        return {
            Intercept(0.12f, -0.40f, nullptr, 0.25f),
            Intercept(0.46f, 0.40f, nullptr, 0.45f),
            Intercept(0.86f, 0.10f, nullptr, 0.65f),
        };
    }

    std::vector<Curve> makeVoiceCurves() {
        return {
            Curve(Intercept(-0.10f, -0.50f), Intercept(-0.05f, -0.25f), Intercept(0.10f, -0.50f)),
            Curve(Intercept(-0.05f, -0.25f), Intercept(0.10f, -0.50f), Intercept(0.42f, 0.25f)),
            Curve(Intercept(0.10f, -0.50f), Intercept(0.42f, 0.25f), Intercept(0.78f, -0.10f)),
            Curve(Intercept(0.42f, 0.25f), Intercept(0.78f, -0.10f), Intercept(1.12f, -0.40f)),
            Curve(Intercept(0.78f, -0.10f), Intercept(1.12f, -0.40f), Intercept(1.46f, 0.40f)),
        };
    }

    void configureFrontState(CycleState& state) {
        state.frontE = Intercept(-0.75f, -0.10f);
        state.frontD = Intercept(-0.55f, -0.20f);
        state.frontC = Intercept(-0.35f, -0.30f);
        state.frontB = Intercept(-0.12f, -0.40f);
        state.frontA = Intercept(-0.04f, -0.50f);
    }

    void requireInterceptNear(const Intercept& actual, const Intercept& expected) {
        REQUIRE(actual.x == Approx(expected.x));
        REQUIRE(actual.y == Approx(expected.y));
        REQUIRE(actual.shp == Approx(expected.shp));
        REQUIRE(actual.adjustedX == Approx(expected.adjustedX));
        REQUIRE(actual.padBefore == expected.padBefore);
        REQUIRE(actual.padAfter == expected.padAfter);
        REQUIRE(actual.isWrapped == expected.isWrapped);
        REQUIRE(actual.cube == expected.cube);
    }

    void requireCurveNear(const Curve& actual, const Curve& expected) {
        requireInterceptNear(actual.a, expected.a);
        requireInterceptNear(actual.b, expected.b);
        requireInterceptNear(actual.c, expected.c);
    }

    void requireCurvesNear(const std::vector<Curve>& actual, const std::vector<Curve>& expected) {
        REQUIRE(actual.size() == expected.size());

        for (int i = 0; i < (int) actual.size(); ++i) {
            INFO("curve=" << i);
            requireCurveNear(actual[i], expected[i]);
        }
    }

    void applyLegacyVoiceCurveResolution(std::vector<Curve>& curves) {
        ::Rasterization::CurveResolutionPolicy::applyResolutionIndices(
                curves,
                0.05f / float(Curve::resolution),
                1);

        int lastIdx = (int) curves.size() - 1;
        curves[0].resIndex = curves[1].resIndex;
        curves[lastIdx].resIndex = curves[lastIdx - 1].resIndex;
    }

    void buildLegacyChainedPadding(
            std::vector<Intercept>& intercepts,
            std::vector<Intercept>& nextIntercepts,
            CycleState& state,
            std::vector<Curve>& curves,
            float interceptPadding) {
        int end = (int) intercepts.size() - 1;

        Intercept back1, back2, back3, back4, back5;
        back1.assignWithTranslation(nextIntercepts[0], 1);
        back2.assignWithTranslation(nextIntercepts[1], 1);
        back3.assignBack(nextIntercepts, 3);
        back4.assignBack(nextIntercepts, 4);
        back5.assignBack(nextIntercepts, 5);

        bool extraPadFront = state.frontB.x > -4 * interceptPadding;
        bool padFront      = state.frontA.x > -4 * interceptPadding;
        bool padBack       = back1.x < 1 + 4 * interceptPadding;
        bool extraPadBack  = back2.x < 1 + 4 * interceptPadding;

        curves.clear();
        curves.reserve(intercepts.size() + 5 + int(extraPadFront) + int(padFront) + int(padBack) + int(extraPadBack));

        if (extraPadFront) {
            curves.emplace_back(state.frontE, state.frontD, state.frontC);
        }

        if (padFront) {
            curves.emplace_back(state.frontD, state.frontC, state.frontB);
        }

        curves.emplace_back(state.frontC, state.frontB, state.frontA);
        curves.emplace_back(state.frontB, state.frontA, intercepts[0]);
        curves.emplace_back(state.frontA, intercepts[0], intercepts[1]);

        for (int i = 0; i < (int) intercepts.size() - 2; ++i) {
            curves.emplace_back(intercepts[i], intercepts[i + 1], intercepts[i + 2]);
        }

        curves.emplace_back(intercepts[end - 1], intercepts[end], back1);
        curves.emplace_back(intercepts[end], back1, back2);
        curves.emplace_back(back1, back2, back3);

        if (padBack) {
            curves.emplace_back(back2, back3, back4);
        }

        if (extraPadBack) {
            curves.emplace_back(back3, back4, back5);
        }

        state.frontE.assignFront(intercepts, 5);
        state.frontD.assignFront(intercepts, 4);
        state.frontC.assignFront(intercepts, 3);
        state.frontB.assignWithTranslation(intercepts[end - 1], -1);
        state.frontA.assignWithTranslation(intercepts[end], -1);
    }
}

TEST_CASE("VoicePointPositionPolicy wraps chained voice phase", "[cycle][rasterization][voice]") {
    Vertex a(0.20f, 0.25f);
    Vertex b(0.80f, 0.75f);
    Vertex vertex;

    a.values[Vertex::Time] = 0.0f;
    a.values[Vertex::Phase] = 0.20f;
    a.values[Vertex::Amp] = 0.25f;
    b.values[Vertex::Time] = 1.0f;
    b.values[Vertex::Phase] = 0.80f;
    b.values[Vertex::Amp] = 0.75f;

    Cycle::Rasterization::VoicePointPositionPolicy::Context context;
    context.voiceTime = 0.5f;
    context.oscPhase = 0.85f;

    auto result = Cycle::Rasterization::VoicePointPositionPolicy().resolve(
            context,
            true,
            &a,
            &b,
            &vertex);

    REQUIRE(result.intersects);
    REQUIRE(result.phase == Approx(0.35f));
    REQUIRE(vertex.values[Vertex::Amp] == Approx(0.50f));
}

TEST_CASE("VoiceChainedPaddingPolicy matches legacy chaining curve padding", "[cycle][rasterization][voice]") {
    std::vector<Intercept> legacyIntercepts = makeCurrentIntercepts();
    std::vector<Intercept> legacyNextIntercepts = makeNextIntercepts();
    CycleState legacyState;
    configureFrontState(legacyState);
    std::vector<Curve> legacyCurves;

    std::vector<Intercept> policyIntercepts = makeCurrentIntercepts();
    std::vector<Intercept> policyNextIntercepts = makeNextIntercepts();
    CycleState policyState;
    configureFrontState(policyState);
    std::vector<Curve> policyCurves;

    constexpr float interceptPadding = 0.05f;

    buildLegacyChainedPadding(
            legacyIntercepts,
            legacyNextIntercepts,
            legacyState,
            legacyCurves,
            interceptPadding);

    int paddingSize = Cycle::Rasterization::VoiceChainedPaddingPolicy().build(
            policyIntercepts,
            policyNextIntercepts,
            policyState,
            policyCurves,
            interceptPadding);

    REQUIRE(paddingSize == 2);
    requireCurvesNear(policyCurves, legacyCurves);
    requireInterceptNear(policyState.frontE, legacyState.frontE);
    requireInterceptNear(policyState.frontD, legacyState.frontD);
    requireInterceptNear(policyState.frontC, legacyState.frontC);
    requireInterceptNear(policyState.frontB, legacyState.frontB);
    requireInterceptNear(policyState.frontA, legacyState.frontA);
}

TEST_CASE("VoiceCurveResolutionPolicy matches legacy voice resolution endpoint handling", "[cycle][rasterization][voice]") {
    std::vector<Curve> legacyCurves = makeVoiceCurves();
    std::vector<Curve> policyCurves = makeVoiceCurves();

    applyLegacyVoiceCurveResolution(legacyCurves);
    Cycle::Rasterization::VoiceCurveResolutionPolicy().apply(policyCurves);

    REQUIRE(policyCurves.size() == legacyCurves.size());

    for (int i = 0; i < (int) policyCurves.size(); ++i) {
        INFO("curve=" << i);
        REQUIRE(policyCurves[i].resIndex == legacyCurves[i].resIndex);
    }

    REQUIRE(policyCurves.front().resIndex == policyCurves[1].resIndex);
    REQUIRE(policyCurves.back().resIndex == policyCurves[policyCurves.size() - 2].resIndex);
}
