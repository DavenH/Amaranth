#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Rasterization/Policies/Core/InterceptRestrictionPolicy.h"
#include "Support/LegacyMeshRasterizer.h"
#include "../src/Util/NumberUtils.h"

namespace {
    using Rasterization::InterceptRestrictionPolicy;

    std::vector<Intercept> makeIntercepts(std::initializer_list<float> xs) {
        std::vector<Intercept> intercepts;

        for (float x : xs) {
            Intercept intercept(x, x + 0.25f);
            intercept.adjustedX = x;
            intercepts.emplace_back(intercept);
        }

        return intercepts;
    }

    std::vector<Intercept> legacyRestrict(
            std::vector<Intercept> intercepts,
            float minimumX,
            float maximumX) {
        for (auto& intercept : intercepts) {
            NumberUtils::constrain(intercept.adjustedX, minimumX, maximumX);
        }

        for (int i = 1; i < (int) intercepts.size(); ++i) {
            Intercept& a = intercepts[i - 1];
            Intercept& b = intercepts[i];

            if (b.adjustedX < a.adjustedX && b.isWrapped == a.isWrapped) {
                b.adjustedX = a.adjustedX + 0.0001f;
            }
        }

        for (auto& intercept : intercepts) {
            intercept.x = intercept.adjustedX;
        }

        if (intercepts.back().x >= maximumX) {
            for (int i = (int) intercepts.size() - 1; i >= 1; --i) {
                Intercept& left = intercepts[i - 1];
                Intercept& right = intercepts[i];

                if (left.x >= right.x) {
                    left.x = right.x - 0.0001f;
                }
            }
        }

        for (int i = 1; i < (int) intercepts.size(); ++i) {
            Intercept& left = intercepts[i - 1];
            Intercept& right = intercepts[i];

            if (right.x <= left.x) {
                right.x = left.x + 0.0001f;
            }
        }

        return intercepts;
    }

    void requireStrictlyIncreasing(const std::vector<Intercept>& intercepts) {
        for (int i = 1; i < (int) intercepts.size(); ++i) {
            INFO("index=" << i
                 << " left=" << intercepts[i - 1].x
                 << " right=" << intercepts[i].x);
            REQUIRE(intercepts[i].x > intercepts[i - 1].x);
        }
    }

    void requireSameXs(
            const std::vector<Intercept>& actual,
            const std::vector<Intercept>& expected) {
        REQUIRE(actual.size() == expected.size());

        for (int i = 0; i < (int) actual.size(); ++i) {
            INFO("index=" << i);
            REQUIRE(actual[i].x == Catch::Approx(expected[i].x).margin(1e-7f));
            REQUIRE(actual[i].adjustedX == Catch::Approx(expected[i].adjustedX).margin(1e-7f));
        }
    }
}

TEST_CASE("InterceptRestrictionPolicy separates duplicate and reversed x values", "[rasterization][restriction]") {
    std::vector<Intercept> intercepts = makeIntercepts({ 0.2f, 0.2f, 0.1f, 1.2f });

    InterceptRestrictionPolicy::Context context;
    context.minimumX = 0.f;
    context.maximumX = 1.f;

    InterceptRestrictionPolicy(context).restrict(intercepts);

    REQUIRE(intercepts.front().x >= 0.f);
    REQUIRE(intercepts.back().x <= 1.f);
    requireStrictlyIncreasing(intercepts);

    REQUIRE(intercepts[1].x - intercepts[0].x == Catch::Approx(0.0001f).margin(1e-7f));
    REQUIRE(intercepts[2].x - intercepts[1].x == Catch::Approx(0.0001f).margin(1e-7f));
}

TEST_CASE("InterceptRestrictionPolicy matches legacy non-cyclic restriction output", "[rasterization][restriction]") {
    std::vector<Intercept> intercepts = makeIntercepts({ -0.2f, 0.15f, 0.14f, 0.75f, 1.5f });
    std::vector<Intercept> expected = legacyRestrict(intercepts, 0.f, 1.f);

    InterceptRestrictionPolicy::Context context;
    context.cyclic = false;
    context.minimumX = 0.f;
    context.maximumX = 1.f;

    InterceptRestrictionPolicy(context).restrict(intercepts);

    requireSameXs(intercepts, expected);
}

TEST_CASE("InterceptRestrictionPolicy matches legacy wrapped-intercept restriction output", "[rasterization][restriction]") {
    std::vector<Intercept> intercepts = makeIntercepts({ 0.8f, 0.1f, 0.2f, 1.1f });
    intercepts[0].isWrapped = true;
    intercepts[1].isWrapped = false;
    intercepts[2].isWrapped = false;
    intercepts[3].isWrapped = false;

    std::vector<Intercept> expected = legacyRestrict(intercepts, 0.f, 1.f);

    InterceptRestrictionPolicy::Context context;
    context.cyclic = true;
    context.minimumX = 0.f;
    context.maximumX = 1.f;

    InterceptRestrictionPolicy(context).restrict(intercepts);

    requireSameXs(intercepts, expected);
}

TEST_CASE("MeshRasterizer restrictIntercepts forwards through restriction policy", "[meshrasterizer][restriction]") {
    MeshRasterizer rasterizer("RestrictionForwardingRasterizer");
    std::vector<Intercept> intercepts = makeIntercepts({ 0.7f, 0.6f, 1.4f });
    std::vector<Intercept> expected = legacyRestrict(intercepts, 0.f, 1.f);

    rasterizer.restrictIntercepts(intercepts);

    requireSameXs(intercepts, expected);
}

TEST_CASE("MeshRasterizer restrictIntercepts accepts empty intercept lists", "[meshrasterizer][restriction]") {
    MeshRasterizer rasterizer("EmptyRestrictionRasterizer");
    std::vector<Intercept> intercepts;

    rasterizer.restrictIntercepts(intercepts);

    REQUIRE(intercepts.empty());
}
