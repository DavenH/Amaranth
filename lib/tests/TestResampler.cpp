#include <catch2/catch_test_macros.hpp>

#include "../src/Algo/Resampler.h"
#include "../src/Array/ScopedAlloc.h"

TEST_CASE("Resampler rejects an input block before its source window overflows",
          "[resampler][regression]") {
    constexpr int inputSize = 1024;
    constexpr int historySize = 32;
    constexpr double outputToInputRatio = 16.;

    Resampler resampler;
    int sourceSize = 0;
    int destinationSize = 0;
    resampler.initWithHistory(
            0.95f,
            9.f,
            1.,
            historySize,
            256,
            inputSize,
            sourceSize,
            destinationSize);

    ScopedAlloc<float> source(sourceSize);
    ScopedAlloc<float> destination(destinationSize);
    ScopedAlloc<float> input(inputSize);
    input.ramp();

    resampler.source = source;
    resampler.dest = destination;
    resampler.reset();
    resampler.primeWithZeros();
    resampler.setRatio(outputToInputRatio);

    REQUIRE_FALSE(resampler.resample(input).empty());
    REQUIRE(resampler.resample(input).empty());
}
