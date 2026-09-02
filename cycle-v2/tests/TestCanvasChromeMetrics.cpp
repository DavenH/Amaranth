#include <catch2/catch_test_macros.hpp>

#include "UI/CanvasChromeMetrics.h"
#include "UI/EditorChromeLayout.h"

using namespace CycleV2;
using juce::Rectangle;

TEST_CASE("Canvas chrome uses one restrained semantic corner scale",
        "[cycle-v2][canvas][chrome][metrics]") {
    REQUIRE(CanvasChromeMetrics::microCornerRadius == 2.f);
    REQUIRE(CanvasChromeMetrics::insetCornerRadius == 3.f);
    REQUIRE(CanvasChromeMetrics::controlCornerRadius == 4.f);
    REQUIRE(CanvasChromeMetrics::tileCornerRadius == 5.f);
    REQUIRE(CanvasChromeMetrics::panelCornerRadius == 6.f);

    REQUIRE(CanvasChromeMetrics::microCornerRadius
            < CanvasChromeMetrics::insetCornerRadius);
    REQUIRE(CanvasChromeMetrics::insetCornerRadius
            < CanvasChromeMetrics::controlCornerRadius);
    REQUIRE(CanvasChromeMetrics::controlCornerRadius
            < CanvasChromeMetrics::tileCornerRadius);
    REQUIRE(CanvasChromeMetrics::tileCornerRadius
            < CanvasChromeMetrics::panelCornerRadius);
}

TEST_CASE("Canvas chrome reserves its strongest generic stroke for focus",
        "[cycle-v2][canvas][chrome][metrics]") {
    REQUIRE(CanvasChromeMetrics::restingBorderWidth == 1.f);
    REQUIRE(CanvasChromeMetrics::activeBorderWidth == 1.5f);
    REQUIRE(CanvasChromeMetrics::focusRingWidth == 2.f);

    REQUIRE(CanvasChromeMetrics::restingBorderWidth
            < CanvasChromeMetrics::activeBorderWidth);
    REQUIRE(CanvasChromeMetrics::activeBorderWidth
            < CanvasChromeMetrics::focusRingWidth);
}

TEST_CASE("Canvas chrome uses one ordered semantic type scale",
        "[cycle-v2][canvas][chrome][metrics]") {
    REQUIRE(CanvasChromeMetrics::microFontSize == 9.f);
    REQUIRE(CanvasChromeMetrics::captionFontSize == 10.5f);
    REQUIRE(CanvasChromeMetrics::labelFontSize == 12.f);
    REQUIRE(CanvasChromeMetrics::sectionTitleFontSize == 14.f);
    REQUIRE(CanvasChromeMetrics::editorTitleFontSize == 18.f);

    REQUIRE(CanvasChromeMetrics::microFontSize
            < CanvasChromeMetrics::captionFontSize);
    REQUIRE(CanvasChromeMetrics::captionFontSize
            < CanvasChromeMetrics::labelFontSize);
    REQUIRE(CanvasChromeMetrics::labelFontSize
            < CanvasChromeMetrics::sectionTitleFontSize);
    REQUIRE(CanvasChromeMetrics::sectionTitleFontSize
            < CanvasChromeMetrics::editorTitleFontSize);
}

TEST_CASE("Full editor headers align titles and actions without overlap",
        "[cycle-v2][editor][chrome][layout]") {
    const Rectangle<int> editor(0, 0, 400, 500);

    const auto withEnabled = fullEditorHeaderLayout(editor, true);
    REQUIRE(withEnabled.header == Rectangle<int>(0, 0, 400, 44));
    REQUIRE(withEnabled.title == Rectangle<int>(18, 8, 296, 28));
    REQUIRE(withEnabled.enabled == Rectangle<int>(322, 8, 28, 28));
    REQUIRE(withEnabled.close == Rectangle<int>(358, 8, 28, 28));
    REQUIRE(withEnabled.title.getRight() < withEnabled.enabled.getX());
    REQUIRE(withEnabled.enabled.getRight() < withEnabled.close.getX());

    const auto withoutEnabled = fullEditorHeaderLayout(editor, false);
    REQUIRE(withoutEnabled.header == Rectangle<int>(0, 0, 400, 44));
    REQUIRE(withoutEnabled.title == Rectangle<int>(18, 8, 332, 28));
    REQUIRE(withoutEnabled.enabled.isEmpty());
    REQUIRE(withoutEnabled.close == Rectangle<int>(358, 8, 28, 28));
    REQUIRE(withoutEnabled.title.getRight() < withoutEnabled.close.getX());

    const auto constrained = fullEditorHeaderLayout(
            Rectangle<int>(0, 0, 180, 120), true);
    REQUIRE(constrained.title == Rectangle<int>(18, 8, 76, 28));
    REQUIRE(constrained.title.getRight() < constrained.enabled.getX());
}

TEST_CASE("Embedded editor headers reserve their compact close affordance",
        "[cycle-v2][editor][chrome][layout]") {
    const auto layout = embeddedEditorHeaderLayout(
            Rectangle<float>(0.f, 0.f, 400.f, 500.f));

    REQUIRE(layout.header == Rectangle<float>(0.f, 0.f, 400.f, 34.f));
    REQUIRE(layout.title == Rectangle<float>(13.f, 4.f, 346.f, 26.f));
    REQUIRE(layout.close == Rectangle<float>(367.f, 6.f, 22.f, 22.f));
    REQUIRE(layout.title.getRight() < layout.close.getX());

    const auto constrained = embeddedEditorHeaderLayout(
            Rectangle<float>(0.f, 0.f, 80.f, 80.f));
    REQUIRE(constrained.title == Rectangle<float>(13.f, 4.f, 26.f, 26.f));
    REQUIRE(constrained.title.getRight() < constrained.close.getX());
}
