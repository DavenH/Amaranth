#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Runtime/PreviewTraversal.h"
#include "UI/SpyPlaybackRipple.h"

using namespace CycleV2;

TEST_CASE("Playback ripple has a precise cusp and symmetric exponential brightness falloff",
        "[cycle-v2][spy-movie][spy-export]") {
    for (const juce::Colour colour : { juce::Colours::black, juce::Colours::grey, juce::Colours::white }) {
        juce::Image image(juce::Image::RGB, 1001, 3, true);
        image.clear(image.getBounds(), colour);
        SpyPlaybackRipple::apply(image, { 0.5f });
        const int peak = image.getPixelAt(500, 1).getRed() - colour.getRed();
        REQUIRE(peak != 0);
        CHECK(std::abs(peak) > std::abs((int) image.getPixelAt(499, 1).getRed() - colour.getRed()));
        CHECK(image.getPixelAt(499, 1) == image.getPixelAt(501, 1));
        const int tail = image.getPixelAt(506, 1).getRed() - colour.getRed();
        CHECK((double) tail / peak == Catch::Approx(std::exp(-1.)).margin(0.025));
        CHECK(image.getPixelAt(494, 1) == image.getPixelAt(506, 1));
        CHECK(image.getPixelAt(500, 1).getBlue() > image.getPixelAt(500, 1).getRed());
        CHECK(image.getPixelAt(463, 1) == colour);
        CHECK(image.getPixelAt(537, 1) == colour);
    }
}

TEST_CASE("Playback ripples stay local and coincident voices do not compound lighting",
        "[cycle-v2][spy-movie][spy-export]") {
    juce::Image base(juce::Image::RGB, 401, 24, true);
    base.clear(base.getBounds(), juce::Colour(80, 90, 60));
    auto single = base.createCopy();
    auto coincident = base.createCopy();
    auto multiple = base.createCopy();
    SpyPlaybackRipple::apply(single, { 0.25f });
    SpyPlaybackRipple::apply(coincident, { 0.25f, 0.25f });
    SpyPlaybackRipple::apply(multiple, { 0.25f, 0.75f });
    for (int y = 0; y < base.getHeight(); ++y) {
        for (int x = 0; x < base.getWidth(); ++x) {
            REQUIRE(single.getPixelAt(x, y) == coincident.getPixelAt(x, y));
            if ((x < 86 || x > 114) && (x < 286 || x > 314)) {
                REQUIRE(multiple.getPixelAt(x, y) == base.getPixelAt(x, y));
            }
        }
    }
    CHECK(multiple.getPixelAt(100, 10) != base.getPixelAt(100, 10));
    CHECK(multiple.getPixelAt(300, 10) != base.getPixelAt(300, 10));
    CHECK(single.getPixelAt(300, 10) == base.getPixelAt(300, 10));
    auto endpoints = base.createCopy();
    SpyPlaybackRipple::apply(endpoints, { 0.f, 1.f });
    CHECK(endpoints.getPixelAt(0, 10).getBlue() > endpoints.getPixelAt(1, 10).getBlue());
    CHECK(endpoints.getPixelAt(400, 10) == endpoints.getPixelAt(0, 10));
    juce::Image pixel(juce::Image::RGB, 1, 1, true);
    SpyPlaybackRipple::apply(pixel, { 1.f });
    CHECK(pixel.getPixelAt(0, 0).getBlue() > 0);
}

TEST_CASE("Movie ripples traverse the saved note-off and release span",
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
