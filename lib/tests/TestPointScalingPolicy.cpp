#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Support/LegacyMeshRasterizer.h"
#include "../src/Curve/Rasterization/Policies/Core/PointScalingPolicy.h"

using Catch::Approx;
using namespace Rasterization;

namespace {
    struct ScalingCase {
        PointScalingMode mode;
        float input;
        float expected;
        float minimum;
        float maximum;
        bool bipolar;
    };
}

TEST_CASE("PointScalingPolicy scales representative values", "[rasterization][scaling]") {
    const ScalingCase cases[] = {
        { PointScalingMode::Unipolar,      0.f,    0.f,   0.f, 1.f, false },
        { PointScalingMode::Unipolar,      0.5f,   0.5f,  0.f, 1.f, false },
        { PointScalingMode::Unipolar,      1.f,    1.f,   0.f, 1.f, false },
        { PointScalingMode::Bipolar,       0.f,   -1.f,  -1.f, 1.f, true  },
        { PointScalingMode::Bipolar,       0.5f,   0.f,  -1.f, 1.f, true  },
        { PointScalingMode::Bipolar,       1.f,    1.f,  -1.f, 1.f, true  },
        { PointScalingMode::HalfBipolar,   0.f,   -0.5f, -1.f, 1.f, true  },
        { PointScalingMode::HalfBipolar,   0.5f,   0.f,  -1.f, 1.f, true  },
        { PointScalingMode::HalfBipolar,   1.f,    0.5f, -1.f, 1.f, true  },
        { PointScalingMode::Identity,     -0.25f, -0.25f, 0.f, 1.f, false },
    };

    for (const auto& testCase : cases) {
        PointScalingPolicy policy(testCase.mode);

        INFO("input=" << testCase.input);
        REQUIRE(policy.scale(testCase.input) == Approx(testCase.expected));
        REQUIRE(policy.minimum() == Approx(testCase.minimum));
        REQUIRE(policy.maximum() == Approx(testCase.maximum));
        REQUIRE(policy.isBipolar() == testCase.bipolar);
    }
}

TEST_CASE("PointScalingPolicy maps legacy MeshRasterizer scaling modes", "[rasterization][scaling]") {
    struct LegacyCase {
        MeshRasterizer::ScalingType legacyType;
        PointScalingMode expectedMode;
        float input;
        float expected;
    };

    const LegacyCase cases[] = {
        { MeshRasterizer::Unipolar,     PointScalingMode::Unipolar,    0.75f, 0.75f },
        { MeshRasterizer::Bipolar,      PointScalingMode::Bipolar,     0.75f, 0.5f  },
        { MeshRasterizer::HalfBipolar,  PointScalingMode::HalfBipolar, 0.75f, 0.25f },
    };

    for (const auto& testCase : cases) {
        PointScalingPolicy policy = PointScalingPolicy::fromLegacyScalingType(testCase.legacyType);

        REQUIRE(policy.getMode() == testCase.expectedMode);
        REQUIRE(policy.scale(testCase.input) == Approx(testCase.expected));
    }
}

TEST_CASE("PointScalingPolicy preserves legacy FX scaling shorthand", "[rasterization][scaling][fx]") {
    REQUIRE(PointScalingPolicy::fromLegacyFxScalingType(MeshRasterizer::Unipolar).scale(0.75f)
            == Approx(0.75f));
    REQUIRE(PointScalingPolicy::fromLegacyFxScalingType(MeshRasterizer::Bipolar).scale(0.75f)
            == Approx(0.5f));
    REQUIRE(PointScalingPolicy::fromLegacyFxScalingType(MeshRasterizer::HalfBipolar).scale(0.75f)
            == Approx(0.5f));
}
