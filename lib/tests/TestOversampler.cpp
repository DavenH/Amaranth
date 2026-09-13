#include <array>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Algo/Oversampler.h>
#include <App/MemoryPool.h>
#include <App/SingletonRepo.h>
#include <Array/ScopedAlloc.h>

namespace {
    float peakAbs(Buffer<float> buffer) {
        float peak = 0.f;

        for (float value : buffer) {
            peak = jmax(peak, std::abs(value));
        }

        return peak;
    }
}

TEST_CASE("Oversampler preserves low frequency amplitude", "[oversampler]") {
    SingletonRepo repo;
    repo.add(new MemoryPool(&repo));

    constexpr int size = 256;
    ScopedAlloc<float> memory(size);

    for (int factor : { 2, 4, 8 }) {
        Buffer<float> buffer(memory.get(), size);

        buffer.sin(1.f / 32.f);
        float before = peakAbs(buffer);

        Oversampler oversampler(&repo, 16);
        oversampler.setOversampleFactor(factor);
        oversampler.startOversamplingBlock(buffer);
        oversampler.stopOversamplingBlock();

        REQUIRE(peakAbs(buffer) == Catch::Approx(before).margin(0.1f));
    }
}

TEST_CASE("Oversampler wraps only the produced downsampled tail", "[oversampler]") {
    SingletonRepo repo;
    repo.add(new MemoryPool(&repo));

    constexpr int sourceSize = 64;
    constexpr int destinationSize = 32;
    constexpr int expectedTailSize = 16;
    std::array<float, sourceSize> sourceWithoutTail{};
    std::array<float, sourceSize> sourceWithTail{};
    std::array<float, destinationSize> withoutTail{};
    std::array<float, destinationSize> withTail{};

    sourceWithoutTail.fill(1.f);
    sourceWithTail.fill(1.f);

    Oversampler withoutTailOversampler(&repo, 16);
    withoutTailOversampler.setOversampleFactor(2);
    withoutTailOversampler.sampleDown(
            { sourceWithoutTail.data(), sourceSize },
            { withoutTail.data(), destinationSize });

    Oversampler withTailOversampler(&repo, 16);
    withTailOversampler.setOversampleFactor(2);
    withTailOversampler.sampleDown(
            { sourceWithTail.data(), sourceSize },
            { withTail.data(), destinationSize },
            true);

    bool wrappedSamplesChanged = false;
    for (int i = 0; i < expectedTailSize; ++i) {
        wrappedSamplesChanged |= withTail[i] != withoutTail[i];
    }
    REQUIRE(wrappedSamplesChanged);

    for (int i = expectedTailSize; i < destinationSize; ++i) {
        REQUIRE(withTail[i] == Catch::Approx(withoutTail[i]));
    }
}
