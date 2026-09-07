#include "Nodes/Envelope/Editor/EnvelopeAxisScaleSelector.h"

#include "UI/Editors/PropertyControls.h"
#include "UI/EnvelopeToolbarMetrics.h"

namespace CycleV2 {

using namespace juce;

namespace {

const Colour kIndicator { 0xffdbe5ef };
constexpr int kScaleLineCount = 4;
constexpr float kLinearPositions[] { 0.f, 0.333f, 0.667f, 1.f };
constexpr float kLogarithmicPositions[] { 0.f, 0.08f, 0.30f, 1.f };

Rectangle<float> scaleDiagramBounds(Rectangle<float> bounds) {
    return bounds.withSizeKeepingCentre(
            EnvelopeToolbarMetrics::diagramCanvasWidth,
            EnvelopeToolbarMetrics::diagramCanvasHeight);
}

void drawScaleDiagram(Graphics& graphics, Rectangle<float> bounds, bool logarithmic) {
    bounds = scaleDiagramBounds(bounds);
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

    Rectangle<float> diagramBounds() const {
        return scaleDiagramBounds(getLocalBounds().toFloat());
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

Rectangle<float> EnvelopeAxisScaleSelector::optionDiagramBounds(
        bool logarithmicOption) const {
    const ScaleButton* button = logarithmicOption
            ? logarithmicButton.get()
            : linearButton.get();
    return button->diagramBounds().translated(
            static_cast<float>(button->getX()),
            static_cast<float>(button->getY()));
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
