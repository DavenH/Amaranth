#include "Nodes/Envelope/Editor/EnvelopeAxisScaleSelector.h"

#include "UI/Editors/PropertyControlLookAndFeel.h"
#include "UI/EnvelopeToolbarMetrics.h"
#include "UiIconData.h"

namespace CycleV2 {

using namespace juce;

namespace {

Rectangle<float> scaleDiagramBounds(Rectangle<float> bounds) {
    return bounds.withSizeKeepingCentre(
            EnvelopeToolbarMetrics::diagramCanvasWidth,
            EnvelopeToolbarMetrics::diagramCanvasHeight);
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
        const auto document = parseXML(String::fromUTF8(logarithmicMode
                ? UiIconData::envelopeScaleLogarithmic : UiIconData::envelopeScaleLinear));
        jassert(document != nullptr);
        if (document != nullptr) {
            icon = Drawable::createFromSVG(*document);
        }
    }

    Rectangle<float> diagramBounds() const {
        return scaleDiagramBounds(getLocalBounds().toFloat());
    }

    void paintButton(Graphics& graphics, bool highlighted, bool down) override {
        paintPanelOverlayButton(graphics, getLocalBounds().toFloat(),
                highlighted, down, hasKeyboardFocus(false), getToggleState(), isEnabled());
        if (icon != nullptr) {
            icon->drawWithin(graphics,
                    getLocalBounds().toFloat().reduced(CanvasChromeMetrics::panelOverlayIconInset),
                    RectanglePlacement::centred, isEnabled() ? 1.f : 0.25f);
        }
    }

    MouseCursor getMouseCursor() override {
        return isEnabled() ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor;
    }

    void enablementChanged() override {
        setTooltip(isEnabled()
                ? logarithmicMode ? "Use logarithmic envelope axis spacing" : "Use linear envelope axis spacing"
                : "Pitch and Scratch envelopes use linear axis spacing");
    }

private:
    std::unique_ptr<Drawable> icon;
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
    setMouseCursor(MouseCursor::NormalCursor);
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

void EnvelopeAxisScaleSelector::resized() {
    const int size = roundToInt(EnvelopeToolbarMetrics::controlWidth);
    linearButton->setBounds(0, 0, size, getHeight());
    logarithmicButton->setBounds(getWidth() - size, 0, size, getHeight());
}

}
