#include "UI/EffectEnableButton.h"

#include "UI/CanvasChromeMetrics.h"
#include "UiIconData.h"

namespace CycleV2 {

using namespace juce;

namespace {

const Drawable* enableIcon() {
    static const std::unique_ptr<Drawable> icon = [] {
        const std::unique_ptr<XmlElement> document = parseXML(UiIconData::effectEnable);
        jassert(document != nullptr);
        return document != nullptr ? Drawable::createFromSVG(*document) : nullptr;
    }();
    return icon.get();
}

}

EffectEnableButton::EffectEnableButton(
        const String& title,
        const String& description,
        const String& tooltip) :
        ToggleButton(title) {
    setButtonText({});
    setTitle(title);
    setDescription(description);
    setTooltip(tooltip);
    setWantsKeyboardFocus(true);
    setMouseCursor(MouseCursor::PointingHandCursor);
}

void EffectEnableButton::paintButton(Graphics& graphics, bool highlighted, bool down) {
    const bool active = getToggleState();
    const Rectangle<float> bounds = getLocalBounds().toFloat().reduced(0.75f);
    const Colour accent { 0xff65b8ff };

    graphics.setColour(active
            ? accent.withAlpha(down ? 0.42f : highlighted ? 0.34f : 0.27f)
            : Colour(0xff0e1318).brighter(down ? 0.12f : highlighted ? 0.06f : 0.f));
    graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::controlCornerRadius);
    graphics.setColour(hasKeyboardFocus(false)
            ? accent
            : active ? accent.withAlpha(0.95f) : Colour(0xff536171));
    graphics.drawRoundedRectangle(
            bounds,
            CanvasChromeMetrics::controlCornerRadius,
            hasKeyboardFocus(false)
                    ? CanvasChromeMetrics::focusRingWidth
                    : active ? CanvasChromeMetrics::activeBorderWidth
                             : CanvasChromeMetrics::restingBorderWidth);

    if (const Drawable* icon = enableIcon()) {
        icon->drawWithin(
                graphics,
                bounds.reduced(6.f),
                RectanglePlacement::centred,
                active ? 1.f : 0.48f);
    }
}

}
