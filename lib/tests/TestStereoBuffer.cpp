#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <memory>
#include <type_traits>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Array/StereoBuffer.h"
#include "../src/Audio/PitchedSample.h"

static_assert(std::is_copy_constructible_v<StereoBuffer>);
static_assert(std::is_copy_assignable_v<StereoBuffer>);

TEST_CASE("StereoBuffer is a shallow stereo view", "[StereoBuffer]") {
    ScopedAlloc<float> memory(8);
    Buffer<float> left = memory.place(4);
    Buffer<float> right = memory.place(4);
    left.set(1.f);
    right.set(2.f);

    StereoBuffer original(left, right);
    StereoBuffer copy = original;
    copy.left[1] = 3.f;
    copy.right[2] = 4.f;

    CHECK(original.numChannels == 2);
    CHECK(original.left[1] == 3.f);
    CHECK(original.right[2] == 4.f);
}

TEST_CASE("StereoBuffer exposes mono and stereo channel views", "[StereoBuffer]") {
    ScopedAlloc<float> memory(12);
    Buffer<float> left = memory.place(6);
    Buffer<float> right = memory.place(6);
    left.ramp();
    right.set(2.f);

    const StereoBuffer mono(left);
    CHECK(mono.numChannels == 1);
    CHECK(mono[0].get() == left.get());

    const StereoBuffer stereo(left, right);
    const StereoBuffer section = stereo.section(2, 3);
    CHECK(section.numChannels == 2);
    CHECK(section.size() == 3);
    CHECK(section[0].get() == left.get() + 2);
    CHECK(section[1].get() == right.get() + 2);
}

TEST_CASE("StereoBuffer applies channel-aware audio operations", "[StereoBuffer]") {
    ScopedAlloc<float> memory(12);
    Buffer<float> destLeft = memory.place(3);
    Buffer<float> destRight = memory.place(3);
    Buffer<float> sourceLeft = memory.place(3);
    Buffer<float> sourceRight = memory.place(3);
    destLeft.set(1.f);
    destRight.set(2.f);
    sourceLeft.set(3.f);
    sourceRight.set(4.f);

    StereoBuffer stereo(destLeft, destRight);
    stereo.add(StereoBuffer(sourceLeft)).mul(0.5f);
    CHECK(stereo.left[0] == 2.f);
    CHECK(stereo.right[0] == 2.5f);
    CHECK(stereo.max() == 2.5f);

    StereoBuffer mono(destLeft);
    mono.add(StereoBuffer(sourceLeft));
    CHECK(mono.left[0] == 5.f);
    CHECK(mono.max() == 5.f);
}

TEST_CASE("PitchedSample owns copied source audio", "[StereoBuffer][PitchedSample]") {
    std::unique_ptr<PitchedSample> sample;

    {
        ScopedAlloc<float> source(4);
        source.ramp();
        sample = std::make_unique<PitchedSample>(source);
        source.set(-1.f);
    }

    REQUIRE(sample != nullptr);
    REQUIRE(sample->audio.numChannels == 1);
    REQUIRE(sample->audio.size() == 4);
    CHECK(sample->audio.left[0] == 0.f);
    CHECK(sample->audio.left[1] == Catch::Approx(1.f / 3.f));
    CHECK(sample->audio.left[2] == Catch::Approx(2.f / 3.f));
    CHECK(sample->audio.left[3] == 1.f);
}
