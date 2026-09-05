#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "UI/CanvasChromePalette.h"
#include "UI/Preview/EffectPlotPalette.h"

using namespace CycleV2;

namespace {

float linearColourComponent(float component) {
    return component <= 0.04045f
            ? component / 12.92f
            : std::pow((component + 0.055f) / 1.055f, 2.4f);
}

float relativeLuminance(juce::Colour colour) {
    return 0.2126f * linearColourComponent(colour.getFloatRed())
            + 0.7152f * linearColourComponent(colour.getFloatGreen())
            + 0.0722f * linearColourComponent(colour.getFloatBlue());
}

float contrastRatio(juce::Colour first, juce::Colour second) {
    const float firstLuminance = relativeLuminance(first);
    const float secondLuminance = relativeLuminance(second);
    const float lighter = juce::jmax(firstLuminance, secondLuminance);
    const float darker = juce::jmin(firstLuminance, secondLuminance);
    return (lighter + 0.05f) / (darker + 0.05f);
}

}

TEST_CASE("Canvas chrome palette preserves one ordered surface hierarchy",
        "[cycle-v2][canvas][chrome][palette]") {
    REQUIRE(CanvasChromePalette::canvasBackground.getARGB() == 0xff101318);
    REQUIRE(CanvasChromePalette::insetBackground.getARGB() == 0xff11171d);
    REQUIRE(CanvasChromePalette::surface.getARGB() == 0xff171d24);
    REQUIRE(CanvasChromePalette::dockSurface.getARGB() == 0xff18212a);
    REQUIRE(CanvasChromePalette::raisedSurface.getARGB() == 0xff202833);

    REQUIRE(relativeLuminance(CanvasChromePalette::canvasBackground)
            < relativeLuminance(CanvasChromePalette::insetBackground));
    REQUIRE(relativeLuminance(CanvasChromePalette::insetBackground)
            < relativeLuminance(CanvasChromePalette::surface));
    REQUIRE(relativeLuminance(CanvasChromePalette::surface)
            < relativeLuminance(CanvasChromePalette::dockSurface));
    REQUIRE(relativeLuminance(CanvasChromePalette::dockSurface)
            < relativeLuminance(CanvasChromePalette::raisedSurface));
}

TEST_CASE("Canvas chrome text and focus retain readable contrast",
        "[cycle-v2][canvas][chrome][palette]") {
    const juce::Colour surfaces[] {
            CanvasChromePalette::canvasBackground,
            CanvasChromePalette::insetBackground,
            CanvasChromePalette::surface,
            CanvasChromePalette::dockSurface,
            CanvasChromePalette::raisedSurface
    };

    for (const juce::Colour surface : surfaces) {
        REQUIRE(contrastRatio(CanvasChromePalette::text, surface) >= 7.f);
        REQUIRE(contrastRatio(CanvasChromePalette::mutedText, surface) >= 4.5f);
        REQUIRE(contrastRatio(CanvasChromePalette::focus, surface) >= 7.f);
    }
}

TEST_CASE("Canvas chrome control states communicate distinct roles",
        "[cycle-v2][canvas][chrome][palette]") {
    const auto resting = CanvasChromePalette::control(
            CanvasChromeControlState::Resting);
    const auto hovered = CanvasChromePalette::control(
            CanvasChromeControlState::Hovered);
    const auto selected = CanvasChromePalette::control(
            CanvasChromeControlState::Selected);
    const auto focused = CanvasChromePalette::control(
            CanvasChromeControlState::Focused);

    REQUIRE(resting.surface != hovered.surface);
    REQUIRE(hovered.surface != selected.surface);
    REQUIRE(resting.border != hovered.border);
    REQUIRE(hovered.border != selected.border);
    REQUIRE(focused.border == CanvasChromePalette::focus);
    REQUIRE(focused.surface == resting.surface);
    REQUIRE(resting.text.getFloatAlpha() < hovered.text.getFloatAlpha());
    REQUIRE(selected.text.getFloatAlpha() >= hovered.text.getFloatAlpha());
}

TEST_CASE("Canvas and effect plots share their common palette family",
        "[cycle-v2][canvas][chrome][palette]") {
    REQUIRE(EffectPlotPalette::background == CanvasChromePalette::canvasBackground);
    REQUIRE(EffectPlotPalette::insetBackground == CanvasChromePalette::insetBackground);
    REQUIRE(EffectPlotPalette::label == CanvasChromePalette::mutedText);
}
