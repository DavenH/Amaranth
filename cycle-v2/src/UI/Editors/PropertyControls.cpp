#include "UI/Editors/PropertyControls.h"

#include "UI/Editors/PropertyControlLookAndFeel.h"

#include <cmath>
#include <cstdlib>

namespace CycleV2 {

using namespace juce;

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
const Colour kInvalid { 0xffdf7272 };
constexpr double kFloatMappingRoundingTolerance = 0.000001;

var boundsToVar(Rectangle<float> bounds) {
    auto* result = new DynamicObject();
    result->setProperty("x", bounds.getX());
    result->setProperty("y", bounds.getY());
    result->setProperty("width", bounds.getWidth());
    result->setProperty("height", bounds.getHeight());
    return result;
}

bool isArrowKey(const KeyPress& key) {
    return key.getKeyCode() == KeyPress::leftKey
            || key.getKeyCode() == KeyPress::rightKey
            || key.getKeyCode() == KeyPress::upKey
            || key.getKeyCode() == KeyPress::downKey;
}

bool increasesValue(const KeyPress& key) {
    return key.getKeyCode() == KeyPress::rightKey
            || key.getKeyCode() == KeyPress::upKey;
}

}

int PropertySliderLayout::usableTrackWidth() const {
    return roundToInt(track.getWidth());
}

PropertySliderLayout propertySliderLayout(
        Rectangle<int> bounds,
        bool showsValue,
        int requestedLabelWidth,
        int gap,
        int requestedValueWidth,
        bool forceCompact) {
    PropertySliderLayout result;
    const int requiredWidth = requestedLabelWidth
            + gap
            + PropertyControlMetrics::minimumUsableTrackWidth
            + (showsValue ? gap + requestedValueWidth : 0)
            + roundToInt(PropertyControlMetrics::thumbWidth);
    result.compact = showsValue && (forceCompact || bounds.getWidth() < requiredWidth);

    if (result.compact) {
        auto heading = bounds.removeFromTop(22);
        result.value = heading.removeFromRight(requestedValueWidth);
        heading.removeFromRight(gap);
        result.label = heading;
        bounds.removeFromTop(4);
        result.slider = bounds.withSizeKeepingCentre(
                bounds.getWidth(),
                PropertyControlMetrics::rowHeight);
    } else {
        result.label = bounds.removeFromLeft(requestedLabelWidth);
        bounds.removeFromLeft(gap);
        if (showsValue) {
            result.value = bounds.removeFromRight(requestedValueWidth);
            bounds.removeFromRight(gap);
        }
        result.slider = bounds.withSizeKeepingCentre(
                bounds.getWidth(),
                PropertyControlMetrics::rowHeight);
    }

    result.track = propertySliderTrackBounds(result.slider.toFloat());
    return result;
}

void stylePropertyLabel(Label& label, const String& text) {
    label.setText(text, dontSendNotification);
    label.setColour(Label::textColourId, kMutedText);
    label.setFont(FontOptions(12.f));
    label.setJustificationType(Justification::centredRight);
}

void stylePropertyButton(TextButton& button, const String& text) {
    button.setButtonText(text);
    button.setColour(TextButton::buttonColourId, Colour(0xff161d25));
    button.setColour(TextButton::buttonOnColourId, Colour(0xff252f3b));
    button.setColour(TextButton::textColourOffId, kText);
    button.setColour(TextButton::textColourOnId, kText);
}

String formatPropertyReal(double value) {
    if (!std::isfinite(value)) {
        return {};
    }
    if (value == std::round(value)) {
        return String(roundToInt(value));
    }

    const double magnitude = std::floor(std::log10(std::abs(value)));
    const int decimalPlaces = jlimit(0, 8, 1 - static_cast<int>(magnitude));
    const double roundingValue = value + std::copysign(
            jmax(1.0, std::abs(value)) * kFloatMappingRoundingTolerance,
            value);
    if (decimalPlaces == 0) {
        return String(roundToInt(roundingValue));
    }
    return String(roundingValue, decimalPlaces)
            .trimCharactersAtEnd("0")
            .trimCharactersAtEnd(".");
}

String formatPropertyPercentage(double value) {
    return formatPropertyReal(value * 100.0) + "%";
}

std::optional<double> parsePropertyNumber(String text, const String& suffix) {
    text = text.trim();
    if (suffix.isNotEmpty() && text.endsWithIgnoreCase(suffix)) {
        text = text.dropLastCharacters(suffix.length()).trimEnd();
    }
    if (text.isEmpty()) {
        return std::nullopt;
    }
    const char* start = text.toRawUTF8();
    char* end {};
    const double result = std::strtod(start, &end);
    if (end == start || *end != '\0' || !std::isfinite(result)) {
        return std::nullopt;
    }
    return result;
}

std::optional<double> parsePropertyPercentage(const String& text) {
    const auto percentage = parsePropertyNumber(text, "%");
    return percentage.has_value()
            ? std::optional<double>(*percentage / 100.0)
            : std::nullopt;
}

String formatPropertyFrequency(double frequency) {
    return frequency >= 1000.0
            ? formatPropertyReal(frequency / 1000.0) + " kHz"
            : String(roundToInt(frequency)) + " Hz";
}

std::optional<double> parsePropertyFrequency(
        String text,
        double minimum,
        double maximum) {
    text = text.trim();
    double multiplier = 1.0;
    if (text.endsWithIgnoreCase("kHz")) {
        text = text.dropLastCharacters(3).trimEnd();
        multiplier = 1000.0;
    } else if (text.endsWithIgnoreCase("Hz")) {
        text = text.dropLastCharacters(2).trimEnd();
    }
    const auto number = parsePropertyNumber(text);
    if (!number.has_value()) {
        return std::nullopt;
    }
    const double frequency = *number * multiplier;
    return frequency >= minimum && frequency <= maximum
            ? std::optional<double>(frequency)
            : std::nullopt;
}

var propertySliderRowAutomationState(const PropertySliderRow& row) {
    const PropertySliderLayout& layout = row.currentLayout();
    auto* result = new DynamicObject();
    result->setProperty("compact", layout.compact);
    result->setProperty("label", boundsToVar(layout.label.toFloat()));
    result->setProperty("slider", boundsToVar(layout.slider.toFloat()));
    result->setProperty("value", boundsToVar(layout.value.toFloat()));
    result->setProperty("track", boundsToVar(layout.track));
    result->setProperty("usableTrackWidth", layout.usableTrackWidth());
    result->setProperty("display", row.valueText());
    result->setProperty("valid", row.valueTextIsValid());
    return result;
}

PrecisionSlider::PrecisionSlider() {
    setSliderStyle(Slider::LinearHorizontal);
    setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    setRange(0.0, 1.0, 0.001);
    setLookAndFeel(&propertyControlLookAndFeel());
    setVelocityBasedMode(false);
    setVelocityModeParameters(
            0.18,
            1,
            0.0,
            true,
            ModifierKeys::shiftModifier);
    setWantsKeyboardFocus(true);
    setMouseCursor(MouseCursor::PointingHandCursor);
}

PrecisionSlider::~PrecisionSlider() {
    setLookAndFeel(nullptr);
}

void PrecisionSlider::setKeyboardSteps(double ordinary, double fine) {
    jassert(ordinary > 0.0);
    jassert(fine > 0.0);
    ordinaryKeyboardStep = ordinary;
    fineKeyboardStep = fine;
}

void PrecisionSlider::setKeyboardStepper(KeyboardStepper stepper) {
    keyboardStepper = std::move(stepper);
}

void PrecisionSlider::setValueSnapper(ValueSnapper snapper) {
    valueSnapper = std::move(snapper);
}

void PrecisionSlider::setLandmarks(std::vector<Landmark> nextLandmarks) {
    landmarks = std::move(nextLandmarks);
    repaint();
}

bool PrecisionSlider::keyPressed(const KeyPress& key) {
    if (!isArrowKey(key)) {
        return Slider::keyPressed(key);
    }
    const ModifierKeys modifiers = key.getModifiers();
    if (modifiers.isAnyModifierKeyDown() && !modifiers.isShiftDown()) {
        return false;
    }

    const bool increase = increasesValue(key);
    const bool fine = modifiers.isShiftDown();
    if (keyboardStepper != nullptr) {
        const double nextValue = keyboardStepper(getValue(), increase, fine);
        applyKeyboardStep(nextValue - getValue());
    } else {
        const double magnitude = fine ? fineKeyboardStep : ordinaryKeyboardStep;
        applyKeyboardStep(increase ? magnitude : -magnitude);
    }
    return true;
}

double PrecisionSlider::snapValue(double attemptedValue, DragMode dragMode) {
    return valueSnapper != nullptr
            ? valueSnapper(attemptedValue, dragMode)
            : attemptedValue;
}

void PrecisionSlider::paint(Graphics& graphics) {
    Slider::paint(graphics);
    if (landmarks.empty()) {
        return;
    }

    const Rectangle<float> track = propertySliderTrackBounds(getLocalBounds().toFloat());
    graphics.setColour(Colour(0xff8793a1).withAlpha(0.72f));
    graphics.setFont(FontOptions(8.f));
    for (const auto& landmark : landmarks) {
        const float x = jmap(
                (float) jlimit(getMinimum(), getMaximum(), landmark.value),
                (float) getMinimum(),
                (float) getMaximum(),
                track.getX(),
                track.getRight());
        graphics.drawVerticalLine(roundToInt(x), track.getY() - 3.f, track.getY());
        const float labelWidth = 36.f;
        const float labelX = jlimit(
                0.f,
                jmax(0.f, (float) getWidth() - labelWidth),
                x - labelWidth * 0.5f);
        graphics.drawText(
                landmark.label,
                Rectangle<float>(
                        labelX,
                        0.f,
                        labelWidth,
                        jmax(0.f, track.getY() - 4.f)),
                Justification::centred);
    }
}

void PrecisionSlider::applyKeyboardStep(double amount) {
    const double nextValue = jlimit(getMinimum(), getMaximum(), getValue() + amount);
    if (approximatelyEqual(nextValue, getValue())) {
        return;
    }
    if (onDragStart) {
        onDragStart();
    }
    setValue(nextValue, sendNotificationSync);
    if (onDragEnd) {
        onDragEnd();
    }
}

PropertySliderRow::PropertySliderRow(Component& owner, const String& labelText) {
    stylePropertyLabel(label, labelText);
    slider.setTitle(labelText);
    value.setVisible(false);
    value.setEditable(true, true, false);
    value.setJustificationType(Justification::centredRight);
    value.setFont(FontOptions(12.f));
    value.setColour(Label::textColourId, kText);
    value.setColour(Label::backgroundColourId, Colour(0xff11171e));
    value.setColour(Label::outlineColourId, Colour(0xff303b48));
    value.setColour(TextEditor::textColourId, kText);
    value.setColour(TextEditor::backgroundColourId, Colour(0xff11171e));
    value.setColour(TextEditor::highlightColourId, Colour(0xff354659));
    value.setWantsKeyboardFocus(true);

    slider.addListener(this);
    value.onTextChange = [this] {
        commitValueText();
    };

    owner.addAndMakeVisible(label);
    owner.addAndMakeVisible(slider);
    owner.addChildComponent(value);
}

PropertySliderRow::~PropertySliderRow() {
    slider.removeListener(this);
}

void PropertySliderRow::setBounds(
        Rectangle<int> bounds,
        int requestedLabelWidth,
        int gap,
        int requestedValueWidth) {
    layout = propertySliderLayout(
            bounds,
            value.isVisible(),
            requestedLabelWidth,
            gap,
            requestedValueWidth,
            forceCompactLayout);
    label.setBounds(layout.label);
    slider.setBounds(layout.slider);
    value.setBounds(layout.value);
    label.setJustificationType(layout.compact
            ? Justification::centredLeft
            : Justification::centredRight);
}

void PropertySliderRow::setCompactLayout(bool shouldUseCompactLayout) {
    forceCompactLayout = shouldUseCompactLayout;
}

void PropertySliderRow::configureValuePresentation(
        ValueFormatter nextFormatter,
        ValueParser nextParser,
        double defaultValue,
        double ordinaryStep,
        double fineStep,
        const String& help) {
    formatter = std::move(nextFormatter);
    parser = std::move(nextParser);
    slider.setKeyboardSteps(ordinaryStep, fineStep);
    slider.setDoubleClickReturnValue(true, defaultValue);
    slider.setDescription(help);
    slider.setTooltip(help);
    value.setTitle(label.getText() + " value");
    value.setDescription(help);
    value.setTooltip(help);
    value.setVisible(true);
    syncValueText();
}

void PropertySliderRow::refreshValueText() {
    invalidValueText = false;
    updateValueState();
    syncValueText();
}

void PropertySliderRow::sliderValueChanged(Slider* changedSlider) {
    if (changedSlider == &slider) {
        syncValueText();
    }
}

void PropertySliderRow::commitValueText() {
    if (syncingValueText || parser == nullptr) {
        return;
    }
    const std::optional<double> parsed = parser(value.getText());
    if (!parsed.has_value() || !std::isfinite(*parsed)
            || *parsed < slider.getMinimum() || *parsed > slider.getMaximum()) {
        invalidValueText = true;
        updateValueState();
        return;
    }

    invalidValueText = false;
    updateValueState();
    if (approximatelyEqual(*parsed, slider.getValue())) {
        syncValueText();
        return;
    }
    if (slider.onDragStart) {
        slider.onDragStart();
    }
    slider.setValue(*parsed, sendNotificationSync);
    if (slider.onDragEnd) {
        slider.onDragEnd();
    }
}

void PropertySliderRow::syncValueText() {
    if (formatter == nullptr || invalidValueText) {
        return;
    }
    const ScopedValueSetter<bool> guard(syncingValueText, true);
    value.setText(formatter(slider.getValue()), dontSendNotification);
}

void PropertySliderRow::updateValueState() {
    value.setColour(
            Label::outlineColourId,
            invalidValueText ? kInvalid : Colour(0xff303b48));
    value.setColour(
            Label::textColourId,
            invalidValueText ? kInvalid : kText);
    value.repaint();
}

}
