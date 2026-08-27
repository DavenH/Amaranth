#pragma once

#include <JuceHeader.h>

#include <functional>
#include <optional>
#include <vector>

namespace CycleV2 {

namespace PropertyControlMetrics {

constexpr int inlineGap = 8;
constexpr int labelWidth = 88;
constexpr int valueWidth = 60;
constexpr int rowHeight = 30;
constexpr int rowGap = 6;
constexpr int sectionGap = 16;
constexpr int compactRowHeight = 56;
constexpr int minimumUsableTrackWidth = 140;
constexpr float visibleTrackHeight = 4.f;
constexpr float thumbWidth = 8.f;
constexpr float thumbHeight = 14.f;

}

struct PropertySliderLayout {
    bool compact {};
    juce::Rectangle<int> label;
    juce::Rectangle<int> slider;
    juce::Rectangle<int> value;
    juce::Rectangle<float> track;

    int usableTrackWidth() const;
};

class PropertySliderRow;

PropertySliderLayout propertySliderLayout(
        juce::Rectangle<int> bounds,
        bool showsValue,
        int labelWidth = PropertyControlMetrics::labelWidth,
        int gap = PropertyControlMetrics::inlineGap,
        int valueWidth = PropertyControlMetrics::valueWidth,
        bool forceCompact = false);

void stylePropertyLabel(juce::Label& label, const juce::String& text);
void stylePropertyButton(juce::TextButton& button, const juce::String& text);
juce::String formatPropertyReal(double value);
std::optional<double> parsePropertyNumber(
        juce::String text,
        const juce::String& suffix = {});
juce::String formatPropertyPercentage(double value);
std::optional<double> parsePropertyPercentage(const juce::String& text);
juce::var propertySliderRowAutomationState(const PropertySliderRow& row);

class PrecisionSlider final : public juce::Slider {
public:
    using KeyboardStepper = std::function<double(double, bool, bool)>;
    using ValueSnapper = std::function<double(double, DragMode)>;

    struct Landmark {
        double value {};
        juce::String label;
    };

    PrecisionSlider();
    ~PrecisionSlider() override;

    void setKeyboardSteps(double ordinary, double fine);
    void setKeyboardStepper(KeyboardStepper stepper);
    void setValueSnapper(ValueSnapper snapper);
    void setLandmarks(std::vector<Landmark> landmarks);
    bool keyPressed(const juce::KeyPress& key) override;
    double snapValue(double attemptedValue, DragMode dragMode) override;
    void paint(juce::Graphics& graphics) override;

private:
    void applyKeyboardStep(double amount);

    double ordinaryKeyboardStep { 0.01 };
    double fineKeyboardStep { 0.001 };
    KeyboardStepper keyboardStepper;
    ValueSnapper valueSnapper;
    std::vector<Landmark> landmarks;
};

class PropertySliderRow : private juce::Slider::Listener {
public:
    using ValueFormatter = std::function<juce::String(double)>;
    using ValueParser = std::function<std::optional<double>(const juce::String&)>;

    PropertySliderRow(juce::Component& owner, const juce::String& labelText);
    ~PropertySliderRow() override;

    void setBounds(
            juce::Rectangle<int> bounds,
            int labelWidth = PropertyControlMetrics::labelWidth,
            int gap = PropertyControlMetrics::inlineGap,
            int valueWidth = PropertyControlMetrics::valueWidth);
    void configureValuePresentation(
            ValueFormatter formatter,
            ValueParser parser,
            double defaultValue,
            double ordinaryKeyboardStep,
            double fineKeyboardStep,
            const juce::String& help);
    void setCompactLayout(bool shouldUseCompactLayout);
    void refreshValueText();

    const PropertySliderLayout& currentLayout() const { return layout; }
    bool valueTextIsValid() const { return !invalidValueText; }
    juce::String valueText() const { return value.getText(); }

    juce::Label label;
    PrecisionSlider slider;
    juce::Label value;

private:
    void sliderValueChanged(juce::Slider* changedSlider) override;
    void commitValueText();
    void syncValueText();
    void updateValueState();

    bool invalidValueText {};
    bool syncingValueText {};
    bool forceCompactLayout {};
    ValueFormatter formatter;
    ValueParser parser;
    PropertySliderLayout layout;
};

}
