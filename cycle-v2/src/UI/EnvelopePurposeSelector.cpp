#include "EnvelopePurposeSelector.h"

#include "EnvelopePurposeIconRenderer.h"

namespace CycleV2 {

namespace {

const Colour kSelectedFill { 0xff2b415a };
const Colour kControlFill { 0xff151c24 };
const Colour kControlBorder { 0xff536171 };

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
        EnvelopePurposeIconRenderer::paint(
                graphics,
                purposeValue,
                getLocalBounds().toFloat().reduced(5.f, 3.f),
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
    clip.addRoundedRectangle(outer, 6.f);

    graphics.setColour(kControlFill);
    graphics.fillPath(clip);
    graphics.saveState();
    graphics.reduceClipRegion(clip);
    for (const auto& button : buttons) {
        if (button->purpose() == selectedPurpose) {
            graphics.setColour(kSelectedFill);
            graphics.fillRect(button->getBounds().toFloat());
        }
    }
    graphics.restoreState();

    graphics.setColour(kControlBorder.withAlpha(0.74f));
    for (size_t index = 1; index < buttons.size(); ++index) {
        const float x = static_cast<float>(buttons[index]->getX());
        graphics.drawVerticalLine(roundToInt(x), outer.getY() + 3.f, outer.getBottom() - 3.f);
    }
    graphics.setColour(kControlBorder.withAlpha(0.82f));
    graphics.drawRoundedRectangle(outer, 6.f, 1.1f);
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
