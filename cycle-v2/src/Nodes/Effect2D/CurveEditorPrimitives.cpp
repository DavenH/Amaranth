#include "CurveEditorPrimitives.h"

#include "CurveNodeModels.h"

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };

class RailLookAndFeel final : public LookAndFeel_V4 {
public:
    void drawLinearSlider(
            Graphics& graphics,
            int x,
            int y,
            int width,
            int height,
            float sliderPosition,
            float,
            float,
            Slider::SliderStyle,
            Slider&) override {
        auto row = Rectangle<float>(
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(width),
                static_cast<float>(height));
        auto rail = row.withSizeKeepingCentre(row.getWidth(), 7.f);
        const float knobX = jlimit(rail.getX(), rail.getRight(), sliderPosition);
        const auto knob = Rectangle<float>(16.f, 16.f).withCentre({ knobX, rail.getCentreY() });

        graphics.setColour(Colour(0xff384351));
        graphics.fillRoundedRectangle(rail, 3.5f);
        graphics.setColour(Colour(0xffdce3ec).withAlpha(0.78f));
        graphics.fillRoundedRectangle(rail.withRight(knobX), 3.5f);
        graphics.setColour(Colour(0xff0d1116));
        graphics.fillEllipse(knob);
        graphics.setColour(Colour(0xffdce3ec));
        graphics.drawEllipse(knob, 1.5f);
    }
};

RailLookAndFeel& railLookAndFeel() {
    static RailLookAndFeel result;
    return result;
}

void styleParameterSlider(Slider& slider) {
    slider.setSliderStyle(Slider::LinearHorizontal);
    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    slider.setRange(0.0, 1.0, 0.001);
    slider.setLookAndFeel(&railLookAndFeel());
}

}

LabeledParameterSlider::LabeledParameterSlider(Component& owner, const String& labelText) {
    styleParameterLabel(label, labelText);
    styleParameterSlider(slider);
    owner.addAndMakeVisible(label);
    owner.addAndMakeVisible(slider);
}

LabeledParameterSlider::~LabeledParameterSlider() {
    slider.setLookAndFeel(nullptr);
}

void LabeledParameterSlider::bind(
        const std::function<void()>& publish,
        const std::function<void()>& beginTransaction,
        const std::function<void()>& commitTransaction) {
    slider.onValueChange = publish;
    slider.onDragStart = beginTransaction;
    slider.onDragEnd = commitTransaction;
}

void LabeledParameterSlider::setBounds(Rectangle<int> bounds, int labelWidth, int gap) {
    label.setBounds(bounds.removeFromLeft(labelWidth));
    bounds.removeFromLeft(gap);
    slider.setBounds(bounds.withTrimmedRight(4).withSizeKeepingCentre(bounds.getWidth() - 4, 26));
}

ParameterToggle::ParameterToggle(Component& owner, const String& labelText) {
    styleParameterLabel(label, labelText);
    button.setButtonText("Enable");
    owner.addAndMakeVisible(label);
    owner.addAndMakeVisible(button);
}

void ParameterToggle::bind(const std::function<void()>& publish) {
    button.onClick = publish;
}

void ParameterToggle::setBounds(Rectangle<int> bounds, int labelWidth, int gap) {
    label.setBounds(bounds.removeFromLeft(labelWidth));
    bounds.removeFromLeft(gap);
    button.setBounds(bounds);
}

void ParameterRail::layout(
        Rectangle<float> area,
        ParameterToggle& enabled,
        std::initializer_list<LabeledParameterSlider*> sliders,
        std::initializer_list<TextButton*> buttons) {
    Rectangle<int> bounds = area.toNearestInt().reduced(12, 8);
    enabled.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(12);

    for (auto* slider : sliders) {
        slider->setBounds(bounds.removeFromTop(32));
        bounds.removeFromTop(12);
    }

    for (auto* button : buttons) {
        auto row = bounds.removeFromTop(32);
        row.removeFromLeft(94);
        button->setBounds(row.removeFromLeft(76).reduced(0, 2));
        bounds.removeFromTop(7);
    }
}

void styleParameterLabel(Label& label, const String& text) {
    label.setText(text, dontSendNotification);
    label.setColour(Label::textColourId, kMutedText);
    label.setFont(FontOptions(12.f, Font::bold));
    label.setJustificationType(Justification::centredRight);
}

void styleParameterButton(TextButton& button, const String& text) {
    button.setButtonText(text);
    button.setColour(TextButton::buttonColourId, Colour(0xff161d25));
    button.setColour(TextButton::buttonOnColourId, Colour(0xff252f3b));
    button.setColour(TextButton::textColourOffId, kText);
    button.setColour(TextButton::textColourOnId, kText);
}

void addEditorParameter(
        std::vector<NodeParameter>& result,
        const Node& node,
        const String& id,
        const String& name,
        const String& value) {
    String resolvedName = name;
    for (const auto& parameter : node.parameters) {
        if (parameter.id == id && parameter.label.isNotEmpty()) {
            resolvedName = parameter.label;
            break;
        }
    }
    result.push_back({ id, resolvedName, value });
}

String retainedEditorParameter(const Node& node, const String& id, const String& fallback) {
    return parameterValueForNode(node, id, fallback);
}

var editorBoundsToVar(Rectangle<float> bounds) {
    auto* result = new DynamicObject();
    result->setProperty("x", bounds.getX());
    result->setProperty("y", bounds.getY());
    result->setProperty("width", bounds.getWidth());
    result->setProperty("height", bounds.getHeight());
    return result;
}

}
