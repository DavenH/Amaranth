#include "UI/Editors/MorphKeyboardShortcutHelp.h"

#include <utility>

#include "UI/CanvasChromePalette.h"
#include "UI/Editors/MorphKeyboardShortcuts.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"

namespace CycleV2 {

using namespace juce;

namespace {

constexpr int kContentWidth = 222;
constexpr int kContentHeight = 74;

}

class MorphKeyboardShortcutHelp::Bubble final : public BubbleComponent {
public:
    explicit Bubble(String shortcutsToUse) :
            shortcuts(std::move(shortcutsToUse)) {
        setTitle("Morph keyboard shortcuts");
        setDescription(shortcuts);
        setAccessible(false);
        setAllowedPlacement(BubbleComponent::above | BubbleComponent::below);
        setColour(
                BubbleComponent::backgroundColourId,
                CanvasChromePalette::raisedSurface.withAlpha(0.98f));
        setColour(
                BubbleComponent::outlineColourId,
                CanvasChromePalette::strongBorder.withAlpha(0.78f));
    }

    void getContentSize(int& width, int& height) override {
        width = kContentWidth;
        height = kContentHeight;
    }

    void paintContent(Graphics& graphics, int width, int height) override {
        Rectangle<float> text { 0.f, 0.f, (float) width, (float) height };
        StringArray rows;
        rows.addLines(shortcuts);
        const float rowHeight = text.getHeight() / (float) jmax(1, rows.size());
        for (const String& row : rows) {
            const int separator = row.indexOfChar(':');
            const String keys = separator >= 0 ? row.substring(0, separator) : row;
            const String description = separator >= 0
                    ? row.substring(separator + 1).trimStart()
                    : String();
            Rectangle<float> rowBounds = text.removeFromTop(rowHeight);
            graphics.setFont(FontOptions(10.5f).withStyle("Bold"));
            graphics.setColour(CanvasChromePalette::focus.withAlpha(0.96f));
            graphics.drawText(
                    keys,
                    rowBounds.removeFromLeft(90.f),
                    Justification::centredLeft,
                    false);
            graphics.setFont(FontOptions(10.5f));
            graphics.setColour(CanvasChromePalette::text.withAlpha(0.92f));
            graphics.drawText(
                    description,
                    rowBounds,
                    Justification::centredLeft,
                    false);
        }
    }

private:
    String shortcuts;
};

class MorphKeyboardShortcutHelp::Target final : public Component {
public:
    Target(MorphKeyboardShortcutHelp& targetOwner, const String& componentId) :
            owner(targetOwner) {
        setComponentID(componentId);
        setName("MorphKeyboardShortcutTarget");
        setTitle("Morph keyboard shortcuts");
        setWantsKeyboardFocus(false);
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
        host    (hostToUse)
    ,   bubble  (std::make_unique<Bubble>(
                morphKeyboardShortcutHelpText(includesYellowAxis)))
    ,   target  (std::make_unique<Target>(*this, componentId)) {
    bubble->setComponentID(componentId + ".bubble");
    bubble->setName("MorphKeyboardShortcutBubble");
    host.addChildComponent(bubble.get());
    host.addChildComponent(target.get());
}

MorphKeyboardShortcutHelp::~MorphKeyboardShortcutHelp() = default;

void MorphKeyboardShortcutHelp::setAnchorBounds(
        Rectangle<float> focusOutlineBounds) {
    iconBounds = keyboardFocusIndicatorBounds(focusOutlineBounds);
    target->setBounds(
            keyboardFocusIndicatorHitBounds(focusOutlineBounds).toNearestInt());
    target->setVisible(true);
    target->toFront(false);
    if (bubble->isVisible()) {
        layoutBubble();
    }
}

void MorphKeyboardShortcutHelp::hide() {
    target->setVisible(false);
    hideBubble();
}

void MorphKeyboardShortcutHelp::showBubble() {
    if (!target->isVisible()) {
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
    bubble->setPosition(iconBounds.toNearestInt(), 8, 7);
}

}
