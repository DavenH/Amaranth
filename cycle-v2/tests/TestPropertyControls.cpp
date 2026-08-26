#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "UI/Editors/PropertyControls.h"

#include <cmath>
#include <cstdlib>

using namespace CycleV2;
using namespace juce;

namespace {

String formatPercentage(double value) {
    return String(value * 100.0, 1) + "%";
}

std::optional<double> parsePercentage(const String& text) {
    String number = text.trim();
    if (number.endsWithChar('%')) {
        number = number.dropLastCharacters(1).trimEnd();
    }
    const char* start = number.toRawUTF8();
    char* end {};
    const double parsed = std::strtod(start, &end);
    if (number.isEmpty() || end == start || *end != '\0' || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed / 100.0;
}

}

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

TEST_CASE("Property slider supports semantic entry, invalid correction, and keyboard precision",
        "[cycle-v2][ui][property-controls][interaction]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    PropertySliderRow row(owner, "Amount");
    row.slider.setRange(0.0, 1.0, 0.00001);
    row.configureValuePresentation(
            formatPercentage,
            parsePercentage,
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
    REQUIRE(row.valueText() == "76.6%");
    REQUIRE(row.valueTextIsValid());
    REQUIRE(row.slider.getDoubleClickReturnValue() == 0.0);

    row.value.setText("37.5%", sendNotificationSync);
    REQUIRE(row.slider.getValue() == Catch::Approx(0.375));
    REQUIRE(row.valueText() == "37.5%");
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
    REQUIRE(row.valueText() == "50.0%");
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
