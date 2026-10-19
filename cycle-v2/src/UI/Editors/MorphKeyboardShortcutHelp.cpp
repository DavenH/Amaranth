#include "UI/Editors/MorphKeyboardShortcutHelp.h"

#include "UI/CanvasChromePalette.h"
#include "UI/Editors/MorphKeyboardShortcuts.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"

namespace CycleV2 {

using namespace juce;

namespace {

constexpr int kContentWidth = 164;
constexpr int kContentHeight = 114;
constexpr int kBubbleMargin = 8;
constexpr int kBubbleArrowLength = 7;
constexpr int kHoverPollHz = 60;
constexpr float kCardCornerRadius = 6.f;
constexpr float kKeycapHeight = 14.f;
constexpr float kKeycapGap = 3.f;
constexpr float kChordGap = 4.f;
constexpr float kChordSeparatorWidth = 7.f;
constexpr float kKeyColumnWidth = 70.f;

float keycapWidth(const String& label) {
    return label.length() > 1 ? 28.f : 16.f;
}

void paintKeycap(
        Graphics& graphics,
        Rectangle<float> bounds,
        const String& label) {
    graphics.setColour(CanvasChromePalette::restingControlSurface);
    graphics.fillRoundedRectangle(bounds, 3.f);
    graphics.setColour(CanvasChromePalette::control(
            CanvasChromeControlState::Resting).border);
    graphics.drawRoundedRectangle(bounds, 3.f, 0.8f);
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.96f));
    graphics.setFont(FontOptions(9.5f).withStyle("Bold"));
    graphics.drawText(label, bounds, Justification::centred, false);
}

void paintShortcutRow(
        Graphics& graphics,
        Rectangle<float> row,
        const StringArray& keys,
        const String& description,
        bool chord = false) {
    Rectangle<float> keyColumn = row.removeFromLeft(kKeyColumnWidth);
    float keyX = keyColumn.getX();

    for (int index = 0; index < keys.size(); ++index) {
        if (index > 0) {
            if (chord) {
                keyX += kChordGap;
                graphics.setColour(
                        CanvasChromePalette::mutedText.withAlpha(0.82f));
                graphics.setFont(FontOptions(9.f));
                graphics.drawText(
                        "+",
                        Rectangle<float> {
                                keyX,
                                row.getY(),
                                kChordSeparatorWidth,
                                row.getHeight()
                        },
                        Justification::centred,
                        false);
                keyX += kChordSeparatorWidth + kChordGap;
            } else {
                keyX += kKeycapGap;
            }
        }

        const float width = keycapWidth(keys[index]);
        paintKeycap(
                graphics,
                { keyX, row.getCentreY() - kKeycapHeight * 0.5f,
                        width, kKeycapHeight },
                keys[index]);
        keyX += width;
    }

    graphics.setColour(CanvasChromePalette::text.withAlpha(0.88f));
    graphics.setFont(FontOptions(10.5f));
    graphics.drawText(description, row, Justification::centredLeft, false);
}

class ShortcutBubbleLookAndFeel final : public LookAndFeel_V4 {
public:
    void drawBubble(
            Graphics& graphics,
            BubbleComponent& component,
            const Point<float>& tip,
            const Rectangle<float>& body) override {
        Path outline;
        const Rectangle<float> card = body.reduced(0.6f);
        outline.addBubble(
                card,
                card.getUnion({ tip.x, tip.y, 1.f, 1.f }),
                tip,
                kCardCornerRadius,
                8.f);

        graphics.setColour(component.findColour(
                BubbleComponent::backgroundColourId));
        graphics.fillPath(outline);
        graphics.setColour(component.findColour(
                BubbleComponent::outlineColourId));
        graphics.strokePath(outline, PathStrokeType(0.8f));
    }
};

}

class MorphKeyboardShortcutHelp::Bubble final : public BubbleComponent {
public:
    explicit Bubble(bool includesYellowAxisToUse) :
            includesYellowAxis(includesYellowAxisToUse) {
        setName("MorphKeyboardShortcutBubble");
        setTitle("Morph keyboard shortcuts");
        setDescription(morphKeyboardShortcutHelpText(includesYellowAxis));
        setAccessible(false);
        setAllowedPlacement(BubbleComponent::above | BubbleComponent::below);
        setLookAndFeel(&lookAndFeel);
        setColour(
                BubbleComponent::backgroundColourId,
                CanvasChromePalette::dockSurface.withAlpha(0.99f));
        setColour(
                BubbleComponent::outlineColourId,
                CanvasChromePalette::strongBorder.withAlpha(0.62f));
    }

    ~Bubble() override {
        setLookAndFeel(nullptr);
    }

    void getContentSize(int& width, int& height) override {
        width = kContentWidth;
        height = kContentHeight;
    }

    void paintContent(Graphics& graphics, int width, int height) override {
        Rectangle<float> content {
                10.f,
                6.f,
                (float) width - 20.f,
                (float) height - 12.f
        };
        Rectangle<float> heading = content.removeFromTop(17.f);
        graphics.setColour(CanvasChromePalette::text.withAlpha(0.96f));
        graphics.setFont(FontOptions(11.f).withStyle("Bold"));
        graphics.drawText("Morph shortcuts", heading, Justification::centredLeft, false);

        graphics.setColour(CanvasChromePalette::border.withAlpha(0.72f));
        graphics.drawHorizontalLine(
                roundToInt(content.getY() + 1.f),
                content.getX(),
                content.getRight());
        content.removeFromTop(5.f);

        const float rowHeight = content.getHeight() / 5.f;
        paintShortcutRow(
                graphics,
                content.removeFromTop(rowHeight),
                includesYellowAxis
                        ? StringArray { "Y", "R", "B" }
                        : StringArray { "R", "B" },
                "Select group");
        paintShortcutRow(
                graphics,
                content.removeFromTop(rowHeight),
                { String::fromUTF8("↑"), String::fromUTF8("↓") },
                "Change focus");
        paintShortcutRow(
                graphics,
                content.removeFromTop(rowHeight),
                { String::fromUTF8("←"), String::fromUTF8("→") },
                "Send to pole");
        paintShortcutRow(
                graphics,
                content.removeFromTop(rowHeight),
                { String::fromUTF8("⇧"), String::fromUTF8("← →") },
                "Move by 20%",
                true);
        paintShortcutRow(
                graphics,
                content,
                { "L" },
                "Toggle link");
    }

private:
    bool includesYellowAxis {};
    ShortcutBubbleLookAndFeel lookAndFeel;
};

class MorphKeyboardShortcutHelp::Target final : public Component {
public:
    Target(MorphKeyboardShortcutHelp& targetOwner, const String& componentId) :
            owner(targetOwner) {
        setComponentID(componentId);
        setName("MorphKeyboardShortcutTarget");
        setTitle("Morph keyboard shortcuts");
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void mouseEnter(const MouseEvent&) override {
        owner.showBubble();
    }

    void mouseMove(const MouseEvent&) override {
        owner.showBubble();
    }

    void mouseExit(const MouseEvent&) override {
        owner.hideBubble();
    }

private:
    MorphKeyboardShortcutHelp& owner;
};

MorphKeyboardShortcutHelp::MorphKeyboardShortcutHelp(
        Component& hostToUse,
        const String& componentId,
        bool includesYellowAxis) :
        bubble  (std::make_unique<Bubble>(includesYellowAxis))
    ,   target  (std::make_unique<Target>(*this, componentId)) {
    bubble->setComponentID(componentId + ".bubble");
    hostToUse.addChildComponent(bubble.get());
    hostToUse.addChildComponent(target.get());
}

MorphKeyboardShortcutHelp::~MorphKeyboardShortcutHelp() {
    stopTimer();
}

void MorphKeyboardShortcutHelp::setAnchorBounds(
        Rectangle<float> focusOutlineBounds) {
    iconBounds = keyboardFocusIndicatorBounds(focusOutlineBounds);
    target->setBounds(
            keyboardFocusIndicatorHitBounds(focusOutlineBounds).toNearestInt());
    target->setVisible(true);
    target->toFront(false);
    if (!isTimerRunning()) {
        startTimerHz(kHoverPollHz);
    }
    if (bubble->isVisible()) {
        layoutBubble();
    }
}

void MorphKeyboardShortcutHelp::hide() {
    stopTimer();
    target->setVisible(false);
    hideBubble();
}

void MorphKeyboardShortcutHelp::showBubble() {
    if (!target->isVisible() || bubble->isVisible()) {
        return;
    }
    layoutBubble();
    bubble->setVisible(true);
    bubble->toFront(false);
    target->toFront(false);
}

void MorphKeyboardShortcutHelp::hideBubble() {
    bubble->setVisible(false);
}

void MorphKeyboardShortcutHelp::layoutBubble() {
    bubble->setPosition(
            iconBounds.toNearestInt(),
            kBubbleMargin,
            kBubbleArrowLength);
}

void MorphKeyboardShortcutHelp::timerCallback() {
    const Point<float> pointer = Desktop::getInstance()
            .getMainMouseSource().getScreenPosition();
    const bool pointerIsOverTarget = target->isShowing()
            && target->getScreenBounds().toFloat().contains(pointer);

    if (pointerIsOverTarget && !bubble->isVisible()) {
        showBubble();
    } else if (!pointerIsOverTarget && bubble->isVisible()) {
        hideBubble();
    }
}

}
