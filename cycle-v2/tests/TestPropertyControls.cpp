#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "UI/Editors/PropertyControlLookAndFeel.h"
#include "UI/Editors/PropertyControls.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Property slider layout preserves useful travel or switches compact form",
        "[cycle-v2][ui][property-controls][geometry]") {
    const PropertySliderLayout ordinary = propertySliderLayout({ 0, 0, 312, 30 }, true);
    REQUIRE_FALSE(ordinary.compact);
    REQUIRE(ordinary.label == Rectangle<int>(0, 0, 88, 30));
    REQUIRE(ordinary.slider == Rectangle<int>(96, 0, 148, 30));
    REQUIRE(ordinary.value == Rectangle<int>(252, 0, 60, 30));
    REQUIRE(ordinary.usableTrackWidth() == PropertyControlMetrics::minimumUsableTrackWidth);
    REQUIRE_FALSE(ordinary.label.intersects(ordinary.slider));
    REQUIRE_FALSE(ordinary.slider.intersects(ordinary.value));

    const PropertySliderLayout compact = propertySliderLayout({ 0, 0, 240, 56 }, true);
    REQUIRE(compact.compact);
    REQUIRE(compact.label == Rectangle<int>(0, 0, 172, 22));
    REQUIRE(compact.value == Rectangle<int>(180, 0, 60, 22));
    REQUIRE(compact.slider == Rectangle<int>(0, 26, 240, 30));
    REQUIRE(compact.usableTrackWidth() == 232);
    REQUIRE_FALSE(compact.label.intersects(compact.value));
    REQUIRE_FALSE(compact.slider.intersects(compact.value));
}

TEST_CASE("Property values use two significant figures without redundant decimals",
        "[cycle-v2][ui][property-controls][formatting]") {
    REQUIRE(formatPropertyReal(0.0) == "0");
    REQUIRE(formatPropertyReal(4.0) == "4");
    REQUIRE(formatPropertyReal(1.49) == "1.5");
    REQUIRE(formatPropertyReal(0.76562) == "0.77");
    REQUIRE(formatPropertyReal(0.05) == "0.05");
    REQUIRE(formatPropertyReal(-24.4) == "-24");
    REQUIRE(formatPropertyPercentage(0.5) == "50%");
    REQUIRE(formatPropertyPercentage(0.76562) == "77%");
}

TEST_CASE("Property slider indicator remains centred at fractional positions",
        "[cycle-v2][ui][property-controls][geometry]") {
    for (float centreX : { 40.f, 40.25f, 40.5f, 40.75f, 41.f }) {
        const Rectangle<float> thumb(
                centreX - PropertyControlMetrics::thumbWidth * 0.5f,
                8.f,
                PropertyControlMetrics::thumbWidth,
                PropertyControlMetrics::thumbHeight);
        const Rectangle<float> indicator = propertySliderIndicatorBounds(thumb);

        REQUIRE(indicator.getCentreX() == Catch::Approx(thumb.getCentreX()));
        REQUIRE(indicator.getCentreY() == Catch::Approx(thumb.getCentreY()));
        REQUIRE(indicator.getWidth() == Catch::Approx(1.f));
    }
}

TEST_CASE("Property slider supports semantic entry, invalid correction, and keyboard precision",
        "[cycle-v2][ui][property-controls][interaction]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    PropertySliderRow row(owner, "Amount");
    row.slider.setRange(0.0, 1.0, 0.00001);
    row.configureValuePresentation(
            formatPropertyPercentage,
            parsePropertyPercentage,
            0.0,
            0.01,
            0.001,
            "Amount help");
    row.setBounds({ 0, 0, 312, 30 });

    int dragStarts = 0;
    int dragEnds = 0;
    row.slider.onDragStart = [&dragStarts] { ++dragStarts; };
    row.slider.onDragEnd = [&dragEnds] { ++dragEnds; };

    row.slider.setValue(0.76562, sendNotificationSync);
    REQUIRE(row.valueText() == "77%");
    REQUIRE(row.valueTextIsValid());
    REQUIRE(row.slider.getDoubleClickReturnValue() == 0.0);

    row.value.setText("37.5%", sendNotificationSync);
    REQUIRE(row.slider.getValue() == Catch::Approx(0.375));
    REQUIRE(row.valueText() == "38%");
    REQUIRE(row.valueTextIsValid());
    REQUIRE(dragStarts == 1);
    REQUIRE(dragEnds == 1);

    row.value.setText("not a value", sendNotificationSync);
    REQUIRE(row.slider.getValue() == Catch::Approx(0.375));
    REQUIRE(row.valueText() == "not a value");
    REQUIRE_FALSE(row.valueTextIsValid());
    REQUIRE(dragStarts == 1);
    REQUIRE(dragEnds == 1);

    row.value.setText("50", sendNotificationSync);
    REQUIRE(row.slider.getValue() == Catch::Approx(0.5));
    REQUIRE(row.valueText() == "50%");
    REQUIRE(row.valueTextIsValid());
    REQUIRE(dragStarts == 2);
    REQUIRE(dragEnds == 2);

    REQUIRE(row.slider.keyPressed(KeyPress(KeyPress::rightKey)));
    REQUIRE(row.slider.getValue() == Catch::Approx(0.51));
    REQUIRE(row.slider.keyPressed(KeyPress(
            KeyPress::rightKey,
            ModifierKeys::shiftModifier,
            0)));
    REQUIRE(row.slider.getValue() == Catch::Approx(0.511));
    REQUIRE(dragStarts == 4);
    REQUIRE(dragEnds == 4);
}

TEST_CASE("Property slider supports semantic keyboard steps and wider values",
        "[cycle-v2][ui][property-controls][interaction]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    PropertySliderRow row(owner, "Amount");
    row.slider.setRange(0.0, 1.0, 0.00001);
    row.configureValuePresentation(
            formatPropertyPercentage,
            parsePropertyPercentage,
            0.0,
            0.01,
            0.001,
            "Amount help");
    row.slider.setKeyboardStepper([](double current, bool increase, bool fine) {
        const double amount = current * current * current;
        const double nextAmount = jlimit(
                0.0,
                1.0,
                amount + (increase ? 1.0 : -1.0) * (fine ? 0.001 : 0.01));
        return std::cbrt(nextAmount);
    });
    row.setBounds({ 0, 0, 324, 30 }, 88, 8, 72);

    REQUIRE_FALSE(row.currentLayout().compact);
    REQUIRE(row.currentLayout().value.getWidth() == 72);
    REQUIRE(row.currentLayout().usableTrackWidth()
            == PropertyControlMetrics::minimumUsableTrackWidth);

    row.slider.setValue(0.5, sendNotificationSync);
    REQUIRE(row.slider.keyPressed(KeyPress(KeyPress::rightKey)));
    REQUIRE(std::pow(row.slider.getValue(), 3.0) == Catch::Approx(0.135).margin(0.00001));
    REQUIRE(row.slider.keyPressed(KeyPress(
            KeyPress::leftKey,
            ModifierKeys::shiftModifier,
            0)));
    REQUIRE(std::pow(row.slider.getValue(), 3.0) == Catch::Approx(0.134).margin(0.00001));
}
