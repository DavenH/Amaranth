#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/GuideCurveTableDsp.h"

#include <vector>

TEST_CASE("Guide curve table DSP applies the Cycle playback contract",
        "[guide][dsp]") {
    std::vector<float> table(GuideCurveProvider::tableSize);
    std::vector<float> noise(GuideCurveProvider::tableSize);
    for (int index = 0; index < GuideCurveProvider::tableSize; ++index) {
        table[(size_t) index] = (float) index / (float) GuideCurveProvider::tableSize;
        noise[(size_t) index] = (float) index * 0.001f;
    }

    GuideCurveTableParameters parameters;
    parameters.noiseLevel = 0.25f;
    parameters.verticalOffsetLevel = 0.5f;
    parameters.phaseOffsetLevel = 0.5f;
    parameters.seed = 17;
    GuideCurveProvider::NoiseContext context;
    context.noiseSeed = 23;
    context.vertOffset = 31;
    context.phaseOffset = 100;

    const int tableModulo = GuideCurveProvider::tableSize - 1;
    const int tableIndex = (int) (0.25f * (float) tableModulo);
    const int phaseOffset = (context.phaseOffset
            & (tableModulo - GuideCurveProvider::tableSize / 2))
            * parameters.phaseOffsetLevel;
    const float expected = table[(size_t) ((tableIndex + phaseOffset) & tableModulo)]
            + parameters.noiseLevel
                    * noise[(size_t) ((context.noiseSeed + parameters.seed) & tableModulo)]
            + parameters.verticalOffsetLevel * noise[(size_t) context.vertOffset];

    const float actual = GuideCurveTableDsp::tableValue(
            { table.data(), (int) table.size() },
            { noise.data(), (int) noise.size() },
            parameters,
            0.25f,
            context);
    REQUIRE(actual == Catch::Approx(expected));
}

TEST_CASE("Guide curve table DSP initializes stable deterministic noise",
        "[guide][dsp]") {
    std::vector<float> first(GuideCurveProvider::tableSize);
    std::vector<float> second(GuideCurveProvider::tableSize);

    GuideCurveTableDsp::initializeNoise({ first.data(), (int) first.size() });
    GuideCurveTableDsp::initializeNoise({ second.data(), (int) second.size() });

    REQUIRE(first == second);
    REQUIRE(GuideCurveTableDsp::stableSeed(0) == 6585);
    REQUIRE(GuideCurveTableDsp::stableSeed(7) == 3528);
}
