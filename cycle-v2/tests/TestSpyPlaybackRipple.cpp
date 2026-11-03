#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Runtime/PreviewTraversal.h"
#include "UI/SpyPlaybackRipple.h"

using namespace CycleV2;

TEST_CASE("Saturation ripple enriches existing colours with symmetric exponential falloff",
        "[cycle-v2][spy-movie][spy-export]") {
    for (const float hue : { 0.05f, 0.35f, 0.60f, 0.95f }) {
        const auto colour = juce::Colour::fromHSL(hue, 0.35f, 0.4f, 1.f);
        const float saturation = colour.getSaturationHSL();
        juce::Image image(juce::Image::RGB, 1001, 3, true);
        image.clear(image.getBounds(), colour);
        SpyPlaybackRipple::apply(image, { 0.5f });
        const auto peak = image.getPixelAt(500, 1);
        CHECK(peak.getSaturationHSL()
                == Catch::Approx(3.f * saturation / (1.f + 2.f * saturation)).margin(0.01));
        CHECK(peak.getSaturationHSL() >= image.getPixelAt(499, 1).getSaturationHSL());
        CHECK(image.getPixelAt(499, 1) == image.getPixelAt(501, 1));
        const auto tail = image.getPixelAt(518, 1);
        const float tailGain = 1.f + 2.f * std::exp(-1.f);
        CHECK(tail.getSaturationHSL() == Catch::Approx(saturation * tailGain
                / (1.f + saturation * (tailGain - 1.f))).margin(0.01));
        CHECK(peak.getSaturationHSL() > tail.getSaturationHSL());
        CHECK(image.getPixelAt(482, 1) == tail);
        CHECK(image.getPixelAt(391, 1) == colour);
        CHECK(image.getPixelAt(609, 1) == colour);
        for (int x = 392; x <= 608; ++x) {
            CHECK(image.getPixelAt(x, 1).getLightness()
                    == Catch::Approx(colour.getLightness()).margin(1. / 255.));
            CHECK(image.getPixelAt(x, 1).getHue()
                    == Catch::Approx(colour.getHue()).margin(0.004));
        }
    }
}

TEST_CASE("Saturation modifier leaves neutral and already fully saturated content unchanged",
        "[cycle-v2][spy-movie][spy-export]") {
    juce::Image image(juce::Image::RGB, 401, 256, true);
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 401; ++x) {
            image.setPixelAt(x, y, juce::Colour((juce::uint8) y, (juce::uint8) y, (juce::uint8) y));
        }
    }
    SpyPlaybackRipple::apply(image, { 0.25f, 0.75f });
    for (int y = 0; y < 256; ++y) {
        CHECK(image.getPixelAt(100, y) == juce::Colour((juce::uint8) y, (juce::uint8) y, (juce::uint8) y));
        CHECK(image.getPixelAt(300, y) == image.getPixelAt(100, y));
    }
    for (const auto colour : { juce::Colours::red, juce::Colours::blue, juce::Colours::yellow }) {
        image.clear(image.getBounds(), colour);
        SpyPlaybackRipple::apply(image, { 0.25f });
        CHECK(image.getPixelAt(100, 100) == colour);
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
            if ((x < 57 || x > 143) && (x < 257 || x > 343)) {
                REQUIRE(multiple.getPixelAt(x, y) == base.getPixelAt(x, y));
            }
        }
    }
    CHECK(multiple.getPixelAt(100, 10) != base.getPixelAt(100, 10));
    CHECK(multiple.getPixelAt(300, 10) != base.getPixelAt(300, 10));
    CHECK(single.getPixelAt(300, 10) == base.getPixelAt(300, 10));
    auto endpoints = base.createCopy();
    SpyPlaybackRipple::apply(endpoints, { 0.f, 1.f });
    CHECK(endpoints.getPixelAt(0, 10).getSaturationHSL()
            >= endpoints.getPixelAt(1, 10).getSaturationHSL());
    CHECK(endpoints.getPixelAt(400, 10) == endpoints.getPixelAt(0, 10));
    juce::Image pixel(juce::Image::RGB, 1, 1, true);
    pixel.clear(pixel.getBounds(), juce::Colour(80, 100, 60));
    SpyPlaybackRipple::apply(pixel, { 1.f });
    CHECK(pixel.getPixelAt(0, 0).getSaturationHSL() > juce::Colour(80, 100, 60).getSaturationHSL());
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
