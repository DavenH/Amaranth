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
