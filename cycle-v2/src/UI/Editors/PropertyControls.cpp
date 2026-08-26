#include "UI/Editors/PropertyControls.h"

#include "UI/Editors/PropertyControlLookAndFeel.h"

#include <cmath>

namespace CycleV2 {

using namespace juce;

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
const Colour kInvalid { 0xffdf7272 };

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
        int requestedValueWidth) {
    PropertySliderLayout result;
    const int requiredWidth = requestedLabelWidth
            + gap
            + PropertyControlMetrics::minimumUsableTrackWidth
            + (showsValue ? gap + requestedValueWidth : 0)
            + roundToInt(PropertyControlMetrics::thumbWidth);
    result.compact = showsValue && bounds.getWidth() < requiredWidth;

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

bool PrecisionSlider::keyPressed(const KeyPress& key) {
    if (!isArrowKey(key)) {
        return Slider::keyPressed(key);
    }
    const ModifierKeys modifiers = key.getModifiers();
    if (modifiers.isAnyModifierKeyDown() && !modifiers.isShiftDown()) {
        return false;
    }

    const double magnitude = modifiers.isShiftDown()
            ? fineKeyboardStep
            : ordinaryKeyboardStep;
    applyKeyboardStep(increasesValue(key) ? magnitude : -magnitude);
    return true;
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
        int gap) {
    layout = propertySliderLayout(
            bounds,
            value.isVisible(),
            requestedLabelWidth,
            gap);
    label.setBounds(layout.label);
    slider.setBounds(layout.slider);
    value.setBounds(layout.value);
    label.setJustificationType(layout.compact
            ? Justification::centredLeft
            : Justification::centredRight);
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
