#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Runtime/PreviewTraversal.h"
#include "UI/SpyPlaybackGlint.h"

using namespace CycleV2;

TEST_CASE("Material glints stay local and coincident voices do not compound lighting",
        "[cycle-v2][spy-movie][spy-export]") {
    juce::Image base(juce::Image::RGB, 400, 24, true);
    for (int y = 0; y < base.getHeight(); ++y) {
        for (int x = 0; x < base.getWidth(); ++x) {
            base.setPixelAt(x, y, juce::Colour((juce::uint8) (80 + (x + y) % 100),
                    (juce::uint8) 90, (juce::uint8) 60));
        }
    }
    auto single = base.createCopy();
    auto coincident = base.createCopy();
    auto multiple = base.createCopy();
    SpyPlaybackGlint::apply(single, { 0.25f });
    SpyPlaybackGlint::apply(coincident, { 0.25f, 0.25f });
    SpyPlaybackGlint::apply(multiple, { 0.25f, 0.75f });
    int changedLeft = 0;
    int changedRight = 0;
    for (int y = 0; y < base.getHeight(); ++y) {
        for (int x = 0; x < base.getWidth(); ++x) {
            REQUIRE(single.getPixelAt(x, y) == coincident.getPixelAt(x, y));
            const bool left = x > 83 && x < 117;
            const bool right = x > 283 && x < 317;
            if (!left && !right) {
                REQUIRE(multiple.getPixelAt(x, y) == base.getPixelAt(x, y));
            }
            changedLeft += left && multiple.getPixelAt(x, y) != base.getPixelAt(x, y);
            changedRight += right && multiple.getPixelAt(x, y) != base.getPixelAt(x, y);
        }
    }
    CHECK(changedLeft > 0);
    CHECK(changedRight > 0);
    CHECK(single.getPixelAt(100, 10) != base.getPixelAt(100, 10));
    CHECK(single.getPixelAt(300, 10) == base.getPixelAt(300, 10));
}

TEST_CASE("Movie glints traverse the saved note-off and release span",
        "[cycle-v2][spy-movie][preview-traversal]") {
    const PreviewTraversal traversal(301, 0.5, 0.6);
    CHECK(traversal.playbackPosition(0., false, 0.) == 0.f);
    CHECK(traversal.playbackPosition(0.3, false, 0.) == Catch::Approx(0.2));
    CHECK(traversal.playbackPosition(2., false, 0.) == Catch::Approx(0.4));
    CHECK(traversal.playbackPosition(0.2, true, 0.) == Catch::Approx(traversal.noteOffPosition()));
    CHECK(traversal.playbackPosition(0.4, true, 0.25) == Catch::Approx(170. / 300.));
    CHECK(traversal.playbackPosition(1., true, 2.) == 1.f);
    const PreviewTraversal noTail(101);
    CHECK(noTail.playbackPosition(0.5, true, 0.3) == Catch::Approx(0.5));
}
