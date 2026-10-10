#include "UI/EnvelopePurposeSelector.h"

#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

class EnvelopePurposeSelector::PurposeButton final : public Button {
public:
    PurposeButton(
            EnvelopePurpose purposeToUse,
            int indexToUse,
            int countToUse) :
            Button          (envelopePurposeLabel(purposeToUse) + " envelope mode")
        ,   purposeValue    (purposeToUse)
        ,   index           (indexToUse)
        ,   count           (countToUse) {
        setComponentID("envelope-mode-" + envelopePurposeToString(purposeValue));
        setTooltip(envelopePurposeLabel(purposeValue));
        setMouseCursor(MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }

    EnvelopePurpose purpose() const { return purposeValue; }

    Rectangle<float> labelBounds() const {
        return getLocalBounds().toFloat().reduced(4.f, 2.f);
    }

    void paintButton(Graphics& graphics, bool highlighted, bool down) override {
        const bool selected = getToggleState();
        if (!selected && (highlighted || down)) {
            Rectangle<float> selectorBounds(
                    (float) -getX(),
                    0.f,
                    (float) getParentWidth(),
                    (float) getHeight());
            selectorBounds = selectorBounds.reduced(0.75f);
            Path hover = propertySegmentPath(selectorBounds, index, count);
            graphics.setColour(Colours::white.withAlpha(down ? 0.14f : 0.08f));
            graphics.fillPath(hover);
        }
        graphics.setColour(Colours::white.withAlpha(
                selected ? 1.f : highlighted ? 0.94f : 0.62f));
        graphics.setFont(12.f);
        graphics.drawText(
                envelopePurposeLabel(purposeValue),
                labelBounds(),
                Justification::centred);
    }

private:
    EnvelopePurpose purposeValue;
    int index {};
    int count {};
};

EnvelopePurposeSelector::EnvelopePurposeSelector() {
    buttons.reserve(kEnvelopePurposes.size());
    for (size_t index = 0; index < kEnvelopePurposes.size(); ++index) {
        const EnvelopePurpose purposeValue = kEnvelopePurposes[index];
        auto button = std::make_unique<PurposeButton>(
                purposeValue,
                (int) index,
                (int) kEnvelopePurposes.size());
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

Rectangle<float> EnvelopePurposeSelector::optionLabelBounds(
        EnvelopePurpose purposeValue) const {
    const PurposeButton* button = buttonFor(purposeValue);
    return button != nullptr
            ? button->labelBounds().translated(
                    static_cast<float>(button->getX()),
                    static_cast<float>(button->getY()))
            : Rectangle<float>();
}

bool EnvelopePurposeSelector::isOptionHovered(EnvelopePurpose purposeValue) const {
    const PurposeButton* button = buttonFor(purposeValue);
    return button != nullptr && button->isMouseOverOrDragging();
}

void EnvelopePurposeSelector::paint(Graphics& graphics) {
    const auto outer = getLocalBounds().toFloat().reduced(0.75f);
    int selectedIndex = -1;
    for (size_t index = 0; index < buttons.size(); ++index) {
        const auto& button = buttons[index];
        if (button->purpose() == selectedPurpose) {
            selectedIndex = (int) index;
            break;
        }
    }
    paintPropertySegmentedControl(
            graphics,
            outer,
            (int) buttons.size(),
            selectedIndex);
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
