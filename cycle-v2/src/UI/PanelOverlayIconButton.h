#pragma once

#include <JuceHeader.h>

#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"

namespace CycleV2 {

class PanelOverlayIconButton final : public juce::Button {
public:
    PanelOverlayIconButton(const juce::String& title, const juce::String& description,
            const juce::String& tooltip, const char* svg) : juce::Button(title) {
        if (const auto document = juce::parseXML(svg)) {
            icon = juce::Drawable::createFromSVG(*document);
        }
        setTitle(title);
        setDescription(description);
        setTooltip(tooltip);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paintButton(juce::Graphics& graphics, bool highlighted, bool down) override {
        paintPanelOverlayButton(graphics, getLocalBounds().toFloat(),
                highlighted, down, hasKeyboardFocus(false), getToggleState(), isEnabled());
        if (icon != nullptr) {
            icon->drawWithin(graphics,
                    getLocalBounds().toFloat().reduced(CanvasChromeMetrics::panelOverlayIconInset),
                    juce::RectanglePlacement::centred, 1.f);
        }
    }

private:
    std::unique_ptr<juce::Drawable> icon;
};

}
