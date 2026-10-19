#include "UI/Editors/MorphKeyboardShortcutHelp.h"

#include "UI/CanvasChromePalette.h"
#include "UI/Editors/MorphKeyboardShortcuts.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"

namespace CycleV2 {

using namespace juce;

namespace {

constexpr int kCardWidth = 180;
constexpr int kCardHeight = 104;
constexpr int kArrowLength = 7;
constexpr float kCardCornerRadius = 6.f;
constexpr float kKeycapHeight = 14.f;
constexpr float kKeycapGap = 3.f;
constexpr float kKeyColumnWidth = 90.f;

float keycapWidth(const String& label) {
    if (label == "Shift") {
        return 32.f;
    }
    return label.length() > 1 ? 40.f : 16.f;
}

void paintKeycap(
        Graphics& graphics,
        Rectangle<float> bounds,
        const String& label) {
    graphics.setColour(CanvasChromePalette::restingControlSurface);
    graphics.fillRoundedRectangle(bounds, 3.f);
    graphics.setColour(CanvasChromePalette::focus.withAlpha(0.48f));
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
        if (chord && index > 0) {
            graphics.setColour(CanvasChromePalette::mutedText.withAlpha(0.82f));
            graphics.setFont(FontOptions(9.f));
            graphics.drawText(
                    "+",
                    Rectangle<float> { keyX, row.getY(), 9.f, row.getHeight() },
                    Justification::centred,
                    false);
            keyX += 9.f;
        }

        const float width = keycapWidth(keys[index]);
        paintKeycap(
                graphics,
                { keyX, row.getCentreY() - kKeycapHeight * 0.5f,
                        width, kKeycapHeight },
                keys[index]);
        keyX += width + kKeycapGap;
    }

    graphics.setColour(CanvasChromePalette::text.withAlpha(0.88f));
    graphics.setFont(FontOptions(10.5f));
    graphics.drawText(description, row, Justification::centredLeft, false);
}

class ShortcutTooltipLookAndFeel final : public LookAndFeel_V4 {
public:
    explicit ShortcutTooltipLookAndFeel(bool includesYellowAxisToUse) :
            includesYellowAxis(includesYellowAxisToUse) {
    }

    void setAnchorBounds(Rectangle<int> bounds) {
        anchorBounds = bounds;
    }

    Rectangle<int> getTooltipBounds(
            const String&,
            Point<int>,
            Rectangle<int> parentArea) override {
        const int fullHeight = kCardHeight + kArrowLength;
        const int maximumX = jmax(
                parentArea.getX(),
                parentArea.getRight() - kCardWidth);
        const int x = jlimit(
                parentArea.getX(),
                maximumX,
                anchorBounds.getCentreX() - kCardWidth / 2);
        appearsBelow = anchorBounds.getBottom() + fullHeight
                        <= parentArea.getBottom()
                || anchorBounds.getY() - fullHeight < parentArea.getY();
        const int preferredY = appearsBelow
                ? anchorBounds.getBottom()
                : anchorBounds.getY() - fullHeight;
        const int y = jlimit(
                parentArea.getY(),
                jmax(parentArea.getY(), parentArea.getBottom() - fullHeight),
                preferredY);
        arrowTipX = (float) jlimit(
                12,
                kCardWidth - 12,
                anchorBounds.getCentreX() - x);
        return { x, y, kCardWidth, fullHeight };
    }

    void drawTooltip(
            Graphics& graphics,
            const String&,
            int width,
            int height) override {
        const Rectangle<float> body = appearsBelow
                ? Rectangle<float> {
                        0.f,
                        (float) kArrowLength,
                        (float) width,
                        (float) height - kArrowLength
                }
                : Rectangle<float> {
                        0.f,
                        0.f,
                        (float) width,
                        (float) height - kArrowLength
                };
        const Point<float> tip {
                arrowTipX,
                appearsBelow ? 0.f : (float) height
        };
        Path outline;
        const Rectangle<float> card = body.reduced(0.6f);
        outline.addBubble(
                card,
                card.getUnion({ tip.x, tip.y, 1.f, 1.f }),
                tip,
                kCardCornerRadius,
                8.f);

        graphics.setColour(CanvasChromePalette::dockSurface.withAlpha(0.99f));
        graphics.fillPath(outline);
        graphics.setColour(CanvasChromePalette::strongBorder.withAlpha(0.62f));
        graphics.strokePath(outline, PathStrokeType(0.8f));

        paintContent(graphics, body);
    }

private:
    void paintContent(Graphics& graphics, Rectangle<float> body) const {
        Rectangle<float> content = body.reduced(10.f, 6.f);
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
                { "Shift", String::fromUTF8("←↑↓→") },
                "Move by 20%",
                true);
        paintShortcutRow(
                graphics,
                content,
                { "L" },
                "Toggle link");
    }

    bool includesYellowAxis {};
    bool appearsBelow { true };
    float arrowTipX { kCardWidth * 0.5f };
    Rectangle<int> anchorBounds;
};

}

class MorphKeyboardShortcutHelp::Tooltip final : public TooltipWindow {
public:
    Tooltip(
            Component& host,
            Component& targetToUse,
            bool includesYellowAxis) :
            TooltipWindow(&host, 0)
        ,   target(targetToUse)
        ,   shortcuts(morphKeyboardShortcutHelpText(includesYellowAxis))
        ,   lookAndFeel(includesYellowAxis) {
        setName("MorphKeyboardShortcutTooltip");
        setTitle("Morph keyboard shortcuts");
        setDescription(shortcuts);
        setAccessible(false);
        setOpaque(false);
        setLookAndFeel(&lookAndFeel);
    }

    ~Tooltip() override {
        setLookAndFeel(nullptr);
    }

    String getTipFor(Component& component) override {
        return &component == &target && target.isShowing()
                ? shortcuts
                : String();
    }

    void setAnchorBounds(Rectangle<int> bounds) {
        lookAndFeel.setAnchorBounds(bounds);
    }

private:
    Component& target;
    String shortcuts;
    ShortcutTooltipLookAndFeel lookAndFeel;
};

class MorphKeyboardShortcutHelp::Target final : public Component {
public:
    explicit Target(const String& componentId) {
        setComponentID(componentId);
        setName("MorphKeyboardShortcutTarget");
        setTitle("Morph keyboard shortcuts");
        setWantsKeyboardFocus(false);
    }
};

MorphKeyboardShortcutHelp::MorphKeyboardShortcutHelp(
        Component& host,
        const String& componentId,
        bool includesYellowAxis) :
        target  (std::make_unique<Target>(componentId))
    ,   tooltip (std::make_unique<Tooltip>(
                host,
                *target,
                includesYellowAxis)) {
    tooltip->setComponentID(componentId + ".bubble");
    host.addChildComponent(target.get());
}

MorphKeyboardShortcutHelp::~MorphKeyboardShortcutHelp() = default;

void MorphKeyboardShortcutHelp::setAnchorBounds(
        Rectangle<float> focusOutlineBounds) {
    const Rectangle<float> iconBounds =
            keyboardFocusIndicatorBounds(focusOutlineBounds);
    target->setBounds(
            keyboardFocusIndicatorHitBounds(focusOutlineBounds).toNearestInt());
    tooltip->setAnchorBounds(iconBounds.toNearestInt());
    target->setVisible(true);
    target->toFront(false);
}

void MorphKeyboardShortcutHelp::hide() {
    target->setVisible(false);
    tooltip->hideTip();
}

}
