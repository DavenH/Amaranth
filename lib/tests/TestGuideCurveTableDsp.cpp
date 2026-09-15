#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/GuideCurveTableDsp.h"

#include <vector>
#include <cstring>

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

TEST_CASE("Prepared guide samples preserve every resolution and dynamic noise operation",
        "[guide][dsp][parity][complexity]") {
    constexpr int tableSize = GuideCurveProvider::tableSize;
    std::vector<float> table(tableSize);
    std::vector<float> noise(tableSize);
    std::vector<float> scratch(tableSize);
    std::vector<float> expected(tableSize);
    std::vector<float> actual(tableSize);
    Buffer<float> tableBuffer(table.data(), tableSize);
    Buffer<float> noiseBuffer(noise.data(), tableSize);
    Buffer<float> scratchBuffer(scratch.data(), tableSize);
    uint32_t seed = 12345;
    tableBuffer.rand(seed).sub(0.5f);
    GuideCurveTableDsp::initializeNoise(noiseBuffer);
    PreparedGuideCurveTable prepared;

    for (int replacement = 0; replacement < 2; ++replacement) {
        tableBuffer.mul(-0.75f);
        prepared.prepare(tableBuffer);
        for (int effects = 0; effects < 8; ++effects) {
            GuideCurveTableParameters parameters;
            parameters.seed = 117;
            parameters.noiseLevel = (effects & 1) ? 0.23f : 0.f;
            parameters.phaseOffsetLevel = (effects & 2) ? 0.73f : 0.f;
            parameters.verticalOffsetLevel = (effects & 4) ? 0.51f : 0.f;
            for (int frameSeed : { 0, 81, tableSize - 1 }) {
                GuideCurveProvider::NoiseContext context;
                context.noiseSeed = frameSeed;
                context.phaseOffset = 711;
                context.vertOffset = 23;
                GuideCurveSamplingWork work;
                const auto compare = [&](int size) {
                    GuideCurveTableDsp::sampleDownAddNoise(
                            tableBuffer, noiseBuffer, scratchBuffer, parameters,
                            { expected.data(), size }, context);
                    GuideCurveTableDsp::sampleDownAddNoise(
                            tableBuffer, noiseBuffer, scratchBuffer, parameters,
                            { actual.data(), size }, context, &prepared, &work);
                    REQUIRE(std::memcmp(expected.data(), actual.data(), (size_t) size * sizeof(float)) == 0);
                };
                for (int ratio = 1; ratio <= PreparedGuideCurveTable::maximumResolutionRatio; ++ratio) {
                    compare(tableSize / ratio);
                }
                REQUIRE(work.preparedCopies == PreparedGuideCurveTable::maximumResolutionRatio);
                REQUIRE(work.downsampleOperations == 0);
                for (int size : { 17, 1000, tableSize - 1 }) {
                    compare(size);
                }
                REQUIRE(work.downsampleOperations == 3);
            }
        }
    }
}
