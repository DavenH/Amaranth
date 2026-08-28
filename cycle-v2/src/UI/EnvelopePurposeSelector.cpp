#include "UI/EnvelopePurposeSelector.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/EnvelopePurposeIconRenderer.h"

namespace CycleV2 {

namespace {

const Colour kSelectedFill { 0xff2b415a };
const Colour kControlFill { 0xff151c24 };
const Colour kControlBorder { 0xff536171 };

Path selectedCellPath(
        Rectangle<float> bounds,
        bool isFirst,
        bool isLast) {
    Path result;
    if (isFirst || isLast) {
        result.addRoundedRectangle(
                bounds.getX(),
                bounds.getY(),
                bounds.getWidth(),
                bounds.getHeight(),
                CanvasChromeMetrics::controlCornerRadius,
                CanvasChromeMetrics::controlCornerRadius,
                isFirst,
                isLast,
                isFirst,
                isLast);
    } else {
        result.addRectangle(bounds);
    }
    return result;
}

}

class EnvelopePurposeSelector::PurposeButton final : public Button {
public:
    explicit PurposeButton(EnvelopePurpose purposeToUse) :
            Button          (envelopePurposeLabel(purposeToUse) + " envelope mode")
        ,   purposeValue    (purposeToUse) {
        setComponentID("envelope-mode-" + envelopePurposeToString(purposeValue));
        setTooltip(envelopePurposeLabel(purposeValue));
        setMouseCursor(MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }

    EnvelopePurpose purpose() const { return purposeValue; }

    void paintButton(Graphics& graphics, bool highlighted, bool) override {
        const bool selected = getToggleState();
        const float opacity = selected ? 1.f : (highlighted ? 0.94f : 0.62f);
        auto iconBounds = getLocalBounds().toFloat().reduced(5.f, 3.f);
        iconBounds = iconBounds.withSizeKeepingCentre(
                iconBounds.getWidth() * 0.85f,
                iconBounds.getHeight() * 0.85f);
        EnvelopePurposeIconRenderer::paint(
                graphics,
                purposeValue,
                iconBounds,
                opacity);
    }

private:
    EnvelopePurpose purposeValue;
};

EnvelopePurposeSelector::EnvelopePurposeSelector() {
    buttons.reserve(kEnvelopePurposes.size());
    for (const EnvelopePurpose purposeValue : kEnvelopePurposes) {
        auto button = std::make_unique<PurposeButton>(purposeValue);
        button->onClick = [this, purposeValue] {
            setPurpose(purposeValue, sendNotificationSync);
        };
        addAndMakeVisible(*button);
        buttons.push_back(std::move(button));
    }
    setPurpose(selectedPurpose);
}

EnvelopePurposeSelector::~EnvelopePurposeSelector() = default;

void EnvelopePurposeSelector::setPurpose(
        EnvelopePurpose nextPurpose,
        NotificationType notification) {
    const bool changed = selectedPurpose != nextPurpose;
    selectedPurpose = nextPurpose;
    for (auto& button : buttons) {
        button->setToggleState(button->purpose() == selectedPurpose, dontSendNotification);
    }
    repaint();
    if (changed && notification != dontSendNotification && onChange) {
        onChange(selectedPurpose);
    }
}

Rectangle<float> EnvelopePurposeSelector::optionBounds(EnvelopePurpose purposeValue) const {
    const PurposeButton* button = buttonFor(purposeValue);
    return button != nullptr ? button->getBounds().toFloat() : Rectangle<float>();
}

bool EnvelopePurposeSelector::isOptionHovered(EnvelopePurpose purposeValue) const {
    const PurposeButton* button = buttonFor(purposeValue);
    return button != nullptr && button->isMouseOverOrDragging();
}

void EnvelopePurposeSelector::paint(Graphics& graphics) {
    const auto outer = getLocalBounds().toFloat().reduced(0.75f);
    Path clip;
    clip.addRoundedRectangle(outer, CanvasChromeMetrics::controlCornerRadius);

    graphics.setColour(kControlFill);
    graphics.fillPath(clip);
    for (size_t index = 0; index < buttons.size(); ++index) {
        const auto& button = buttons[index];
        if (button->purpose() == selectedPurpose) {
            graphics.setColour(kSelectedFill);
            graphics.fillPath(selectedCellPath(
                    button->getBounds().toFloat().getIntersection(outer),
                    index == 0,
                    index + 1 == buttons.size()));
        }
    }

    graphics.setColour(kControlBorder.withAlpha(0.74f));
    for (size_t index = 1; index < buttons.size(); ++index) {
        const float x = static_cast<float>(buttons[index]->getX());
        graphics.drawVerticalLine(roundToInt(x), outer.getY() + 3.f, outer.getBottom() - 3.f);
    }
    graphics.setColour(kControlBorder.withAlpha(0.82f));
    graphics.drawRoundedRectangle(outer, CanvasChromeMetrics::controlCornerRadius, 1.1f);
}

void EnvelopePurposeSelector::resized() {
    int left = 0;
    for (size_t index = 0; index < buttons.size(); ++index) {
        const int right = roundToInt(
                static_cast<float>(getWidth()) * static_cast<float>(index + 1)
                / static_cast<float>(buttons.size()));
        buttons[index]->setBounds(left, 0, right - left, getHeight());
        left = right;
    }
}

EnvelopePurposeSelector::PurposeButton* EnvelopePurposeSelector::buttonFor(
        EnvelopePurpose purposeValue) const {
    for (const auto& button : buttons) {
        if (button->purpose() == purposeValue) {
            return button.get();
        }
    }
    return nullptr;
}

}
