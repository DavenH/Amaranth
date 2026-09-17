#include "UI/Editors/PropertySegmentedSelector.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"
#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

using namespace juce;

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kHoverFill { 0xffdbe5ef };

}

class PropertySegmentedSelector::OptionButton final : public TextButton {
public:
    OptionButton(
            PropertySegmentedSelector& ownerToUse,
            int indexToUse,
            int countToUse,
            const PropertySegmentOption& option) :
            TextButton (option.label)
        ,   owner  (ownerToUse)
        ,   index  (indexToUse)
        ,   count  (countToUse)
        ,   label  (option.label) {
        setComponentID(option.componentId);
        setTitle(option.label);
        setDescription(option.tooltip);
        setTooltip(option.tooltip);
        setMouseCursor(MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }

    void paintButton(Graphics& graphics, bool highlighted, bool down) override {
        const Rectangle<float> bounds = getLocalBounds().toFloat();
        const Rectangle<float> selectorBounds {
            (float) -getX(),
            (float) -getY(),
            (float) owner.getWidth(),
            (float) owner.getHeight()
        };
        if (!getToggleState() && (highlighted || down)) {
            graphics.setColour(kHoverFill.withAlpha(down ? 0.14f : 0.08f));
            graphics.fillPath(propertySegmentPath(
                    selectorBounds.reduced(0.75f), index, count));
        }
        graphics.setColour(kText.withAlpha(
                getToggleState() ? 1.f : highlighted || down ? 0.86f : 0.66f));
        graphics.setFont(FontOptions(10.f).withStyle(
                getToggleState() ? "Bold" : "Regular"));
        graphics.drawText(label, getLocalBounds(), Justification::centred);

        if (hasKeyboardFocus(false)) {
            graphics.setColour(propertyControlFocusColour().withAlpha(0.82f));
            graphics.strokePath(
                    propertySegmentPath(selectorBounds.reduced(1.5f), index, count),
                    PathStrokeType(CanvasChromeMetrics::focusRingWidth));
        }
    }

    bool keyPressed(const KeyPress& key) override {
        if (key == KeyPress::leftKey) {
            owner.focus(index - 1);
            return true;
        }
        if (key == KeyPress::rightKey) {
            owner.focus(index + 1);
            return true;
        }
        if (key.getKeyCode() == KeyPress::spaceKey) {
            return false;
        }
        if (key == KeyPress::returnKey) {
            owner.select(index, sendNotificationSync);
            return true;
        }
        return Button::keyPressed(key);
    }

private:
    PropertySegmentedSelector& owner;
    int index {};
    int count {};
    String label;
};

PropertySegmentedSelector::PropertySegmentedSelector(
        std::vector<PropertySegmentOption> options) :
        optionDefinitions(std::move(options)) {
    jassert(optionDefinitions.size() > 1);
    buttons.reserve(optionDefinitions.size());
    for (size_t index = 0; index < optionDefinitions.size(); ++index) {
        auto button = std::make_unique<OptionButton>(
                *this,
                (int) index,
                (int) optionDefinitions.size(),
                optionDefinitions[index]);
        button->onClick = [this, index] {
            select((int) index, sendNotificationSync);
        };
        addAndMakeVisible(*button);
        buttons.push_back(std::move(button));
    }
    select(0, dontSendNotification);
}

PropertySegmentedSelector::~PropertySegmentedSelector() = default;

void PropertySegmentedSelector::setSelectedValue(
        const String& nextValue,
        NotificationType notification) {
    for (size_t index = 0; index < optionDefinitions.size(); ++index) {
        if (optionDefinitions[index].value == nextValue) {
            select((int) index, notification);
            return;
        }
    }
}

int PropertySegmentedSelector::selectedIndex() const {
    for (size_t index = 0; index < optionDefinitions.size(); ++index) {
        if (optionDefinitions[index].value == value) {
            return (int) index;
        }
    }
    return -1;
}

Rectangle<float> PropertySegmentedSelector::optionBounds(int index) const {
    return isPositiveAndBelow(index, (int) buttons.size())
            ? buttons[(size_t) index]->getBounds().toFloat()
            : Rectangle<float>();
}

void PropertySegmentedSelector::paint(Graphics& graphics) {
    paintPropertySegmentedControl(
            graphics,
            getLocalBounds().toFloat().reduced(0.75f),
            (int) buttons.size(),
            selectedIndex());
}

void PropertySegmentedSelector::resized() {
    int left = 0;
    for (size_t index = 0; index < buttons.size(); ++index) {
        const int right = roundToInt(
                (float) getWidth() * (float) (index + 1) / (float) buttons.size());
        buttons[index]->setBounds(left, 0, right - left, getHeight());
        left = right;
    }
}

void PropertySegmentedSelector::select(
        int index,
        NotificationType notification) {
    if (!isPositiveAndBelow(index, (int) optionDefinitions.size())) {
        return;
    }
    const String nextValue = optionDefinitions[(size_t) index].value;
    value = nextValue;
    for (size_t buttonIndex = 0; buttonIndex < buttons.size(); ++buttonIndex) {
        buttons[buttonIndex]->setToggleState(
                buttonIndex == (size_t) index,
                dontSendNotification);
    }
    repaint();
    if (notification != dontSendNotification && onChange) {
        onChange(value);
    }
}

void PropertySegmentedSelector::focus(int index) {
    const int target = jlimit(0, (int) buttons.size() - 1, index);
    buttons[(size_t) target]->grabKeyboardFocus();
}

}
