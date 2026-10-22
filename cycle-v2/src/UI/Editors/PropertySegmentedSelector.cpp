#include "UI/Editors/PropertySegmentedSelector.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
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
        ,   label  (option.label)
        ,   ticks  (option.ticks) {
        setComponentID(option.componentId);
        setTitle(option.label);
        setDescription(option.tooltip);
        setTooltip(option.tooltip);
        setMouseCursor(MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }

    String getTooltip() override {
        return owner.hoverMessageProvider ? String() : TextButton::getTooltip();
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
        graphics.drawText(label, getLocalBounds().withTrimmedBottom(ticks.empty() ? 0 : 4), Justification::centred);
        const float start = (getWidth() - (float) ticks.size() * 9.f + 3.f) * 0.5f;
        for (size_t tick = 0; tick < ticks.size(); ++tick) {
            graphics.setColour(ticks[tick].withAlpha(getToggleState() ? 1.f : 0.7f));
            graphics.fillRoundedRectangle(start + tick * 9.f, getHeight() - 5.f, 6.f, 2.f, 1.f);
        }

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
    std::vector<Colour> ticks;
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

void PropertySegmentedSelector::setHoverMessageProvider(
        std::function<String(const String&)> provider) {
    hoverMessageProvider = std::move(provider);
    if (hoverMessageProvider) {
        startTimerHz(20);
    } else {
        stopTimer();
        hoverCallout.reset();
    }
}

void PropertySegmentedSelector::timerCallback() {
    const auto pointer = Desktop::getInstance().getMainMouseSource().getScreenPosition();
    int nextIndex = -1;
    if (isShowing() && Process::isForegroundProcess()
            && !ModifierKeys::currentModifiers.isAnyMouseButtonDown()) {
        for (size_t index = 0; index < buttons.size(); ++index) {
            if ((int) index != selectedIndex() && buttons[index]->getScreenBounds().toFloat().contains(pointer)) {
                nextIndex = (int) index;
                break;
            }
        }
    }
    const uint32_t now = Time::getMillisecondCounter();
    if (nextIndex != hoveredIndex) {
        hoveredIndex = nextIndex;
        hoverStarted = now;
        if (hoverCallout != nullptr) {
            hoverCallout->setVisible(false);
        }
    }
    if (hoveredIndex < 0 || now - hoverStarted < 500 || getParentComponent() == nullptr
            || (hoverCallout != nullptr && hoverCallout->isVisible())) {
        return;
    }
    const String message = hoverMessageProvider(optionDefinitions[(size_t) hoveredIndex].value);
    if (message.isEmpty()) {
        return;
    }
    if (hoverCallout == nullptr) {
        hoverCallout = std::make_unique<BubbleMessageComponent>(0);
        hoverCallout->setComponentID(getComponentID() + ".hoverCallout");
        hoverCallout->setAccessible(false);
        hoverCallout->setInterceptsMouseClicks(false, false);
        hoverCallout->setAllowedPlacement(BubbleComponent::below);
        hoverCallout->setColour(BubbleComponent::backgroundColourId, CanvasChromePalette::dockSurface);
        hoverCallout->setColour(BubbleComponent::outlineColourId, CanvasChromePalette::strongBorder);
    }
    getParentComponent()->addChildComponent(*hoverCallout);
    hoverCallout->setDescription(message);
    AttributedString text;
    text.append(message, Font(FontOptions(11.f)), CanvasChromePalette::text);
    hoverCallout->showAt(buttons[(size_t) hoveredIndex].get(), text, 0, true, false);
    hoverCallout->toFront(false);
}

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
    if (value != nextValue) {
        hoveredIndex = -1;
        if (hoverCallout != nullptr) {
            hoverCallout->setVisible(false);
        }
    }
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
