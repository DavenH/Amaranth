#include "Nodes/Envelope/Editor/EnvelopeAxisScaleSelector.h"

#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

using namespace juce;

namespace {

const Colour kIndicator { 0xffdbe5ef };
constexpr int kScaleLineCount = 4;
constexpr float kLinearPositions[] { 0.f, 0.333f, 0.667f, 1.f };
constexpr float kLogarithmicPositions[] { 0.f, 0.08f, 0.30f, 1.f };

void drawScaleDiagram(Graphics& graphics, Rectangle<float> bounds, bool logarithmic) {
    bounds.reduce(6.f, 5.f);
    graphics.setColour(kIndicator.withAlpha(0.82f));
    const float* positions = logarithmic
            ? kLogarithmicPositions
            : kLinearPositions;
    for (int index = 0; index < kScaleLineCount; ++index) {
        const float position = positions[index];
        const float y = bounds.getY() + position * bounds.getHeight();
        graphics.drawHorizontalLine(
                roundToInt(y),
                bounds.getX(),
                bounds.getRight());
    }
}

}

class EnvelopeAxisScaleSelector::ScaleButton final : public Button {
public:
    explicit ScaleButton(bool logarithmicToUse) :
            Button          (logarithmicToUse
                    ? "Logarithmic axis scale"
                    : "Linear axis scale")
        ,   logarithmicMode (logarithmicToUse) {
        setComponentID(logarithmicMode
                ? "envelope-axis-scale-logarithmic"
                : "envelope-axis-scale-linear");
        setTooltip(logarithmicMode
                ? "Use logarithmic envelope axis spacing"
                : "Use linear envelope axis spacing");
        setMouseCursor(MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }

    void paintButton(Graphics& graphics, bool highlighted, bool) override {
        drawScaleDiagram(
                graphics,
                getLocalBounds().toFloat(),
                logarithmicMode);
        if (getToggleState()) {
            graphics.setColour(kIndicator.withAlpha(0.92f));
            Rectangle<float> indicator = getLocalBounds().toFloat().removeFromBottom(2.f);
            graphics.fillRect(indicator.reduced(4.f, 0.f));
        } else if (highlighted) {
            graphics.setColour(kIndicator.withAlpha(0.12f));
            graphics.fillRect(getLocalBounds().toFloat().reduced(2.f));
        }
    }

private:
    bool logarithmicMode {};
};

EnvelopeAxisScaleSelector::EnvelopeAxisScaleSelector() :
        linearButton      (std::make_unique<ScaleButton>(false))
    ,   logarithmicButton (std::make_unique<ScaleButton>(true)) {
    linearButton->onClick = [this] {
        setLogarithmic(false, sendNotificationSync);
    };
    logarithmicButton->onClick = [this] {
        setLogarithmic(true, sendNotificationSync);
    };
    addAndMakeVisible(*linearButton);
    addAndMakeVisible(*logarithmicButton);
    setLogarithmic(false);
}

EnvelopeAxisScaleSelector::~EnvelopeAxisScaleSelector() = default;

void EnvelopeAxisScaleSelector::setLogarithmic(
        bool shouldBeLogarithmic,
        NotificationType notification) {
    const bool changed = logarithmic != shouldBeLogarithmic;
    logarithmic = shouldBeLogarithmic;
    linearButton->setToggleState(!logarithmic, dontSendNotification);
    logarithmicButton->setToggleState(logarithmic, dontSendNotification);
    repaint();

    if (changed && notification != dontSendNotification && onChange) {
        onChange(logarithmic);
    }
}

Rectangle<float> EnvelopeAxisScaleSelector::optionBounds(bool logarithmicOption) const {
    return (logarithmicOption ? logarithmicButton : linearButton)->getBounds().toFloat();
}

void EnvelopeAxisScaleSelector::paint(Graphics& graphics) {
    const Rectangle<float> outer = getLocalBounds().toFloat().reduced(0.75f);
    paintPropertySegmentedControl(
            graphics,
            outer,
            2,
            logarithmic ? 1 : 0);
}

void EnvelopeAxisScaleSelector::resized() {
    const int split = getWidth() / 2;
    linearButton->setBounds(0, 0, split, getHeight());
    logarithmicButton->setBounds(split, 0, getWidth() - split, getHeight());
}

}
