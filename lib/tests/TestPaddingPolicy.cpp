#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Curve.h"
#include "../src/Curve/FXRasterizer.h"
#include "../src/Curve/MeshRasterizer.h"
#include "../src/Curve/Rasterization/Policies/Core/PaddingPolicy.h"

namespace {
    struct CurveTableScope {
        CurveTableScope() {
            if (refCount++ == 0) {
                Curve::calcTable();
            }
        }

        ~CurveTableScope() {
            if (--refCount == 0) {
                Curve::deleteTable();
            }
        }

        inline static int refCount = 0;
    };

    std::vector<Intercept> makeIntercepts() {
        std::vector<Intercept> intercepts {
            Intercept(0.10f, 0.20f, nullptr, 0.30f),
            Intercept(0.35f, 0.75f, nullptr, 0.40f),
            Intercept(0.70f, 0.40f, nullptr, 0.80f),
            Intercept(0.92f, 0.62f, nullptr, 0.50f),
        };

        for (auto& intercept : intercepts) {
            intercept.adjustedX = intercept.x;
        }

        return intercepts;
    }

    void requireInterceptNear(const Intercept& actual, const Intercept& expected) {
        REQUIRE(actual.x == Catch::Approx(expected.x));
        REQUIRE(actual.y == Catch::Approx(expected.y));
        REQUIRE(actual.shp == Catch::Approx(expected.shp));
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
}

TEST_CASE("CyclicPaddingPolicy matches MeshRasterizer wrapped padding", "[rasterization][padding]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> intercepts = makeIntercepts();

    MeshRasterizer rasterizer("PaddingPolicyWrappedReference");
    std::vector<Curve> forwardedCurves;
    rasterizer.padIcptsWrapped(intercepts, forwardedCurves);

    Rasterization::PaddingPolicyContext context;
    std::vector<Curve> policyCurves;
    std::vector<Intercept> frontPadding;
    std::vector<Intercept> backPadding;
    int paddingSize = Rasterization::CyclicPaddingPolicy(context).build(
            intercepts,
            policyCurves,
            frontPadding,
            backPadding);

    REQUIRE(paddingSize == rasterizer.getPaddingSize());
    REQUIRE(frontPadding.size() == rasterizer.getFrontIcpts().size());
    REQUIRE(backPadding.size() == rasterizer.getBackIcpts().size());

    for (int i = 0; i < (int) frontPadding.size(); ++i) {
        INFO("frontPadding=" << i);
        requireInterceptNear(frontPadding[i], rasterizer.getFrontIcpts()[i]);
    }

    for (int i = 0; i < (int) backPadding.size(); ++i) {
        INFO("backPadding=" << i);
        requireInterceptNear(backPadding[i], rasterizer.getBackIcpts()[i]);
    }

    requireCurvesNear(policyCurves, forwardedCurves);
}

TEST_CASE("NonCyclicPaddingPolicy matches MeshRasterizer non-cyclic padding", "[rasterization][padding]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> intercepts = makeIntercepts();

    MeshRasterizer rasterizer("PaddingPolicyNonCyclicReference");
    rasterizer.setWrapsEnds(false);
    std::vector<Curve> forwardedCurves;
    rasterizer.padIcpts(intercepts, forwardedCurves);

    Rasterization::PaddingPolicyContext context;
    std::vector<Intercept> emptyBounds;
    context.minimumX = 0.f;
    context.maximumX = 1.f;
    context.boundsIntercepts = &emptyBounds;

    std::vector<Curve> policyCurves;
    int paddingSize = Rasterization::NonCyclicPaddingPolicy(context).build(intercepts, policyCurves);

    REQUIRE(paddingSize == rasterizer.getPaddingSize());
    REQUIRE(policyCurves.front().a.x == Catch::Approx(-0.49f));
    REQUIRE(policyCurves.back().c.x == Catch::Approx(1.49f));
    requireCurvesNear(policyCurves, forwardedCurves);
}

TEST_CASE("FxPaddingPolicy matches FXRasterizer padding", "[rasterization][padding][fx]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> intercepts = makeIntercepts();

    FXRasterizer rasterizer(nullptr, "PaddingPolicyFxReference");
    std::vector<Curve> forwardedCurves;
    rasterizer.padIcpts(intercepts, forwardedCurves);

    std::vector<Curve> policyCurves;
    int paddingSize = Rasterization::FxPaddingPolicy().build(intercepts, policyCurves);

    REQUIRE(paddingSize == rasterizer.getPaddingSize());
    REQUIRE(policyCurves.front().a.x == Catch::Approx(-1.0f));
    REQUIRE(policyCurves.front().b.x == Catch::Approx(-0.5f));
    REQUIRE(policyCurves.back().b.x == Catch::Approx(1.5f));
    REQUIRE(policyCurves.back().c.x == Catch::Approx(2.0f));
    requireCurvesNear(policyCurves, forwardedCurves);
}
