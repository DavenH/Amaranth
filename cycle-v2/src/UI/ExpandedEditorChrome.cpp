#include "UI/ExpandedEditorChrome.h"

#include <utility>

#include "UI/CanvasChromeMetrics.h"
#include "UI/EditorChromeLayout.h"

namespace CycleV2 {

using namespace juce;

ExpandedEditorChrome::ExpandedEditorChrome(
        Component& ownerToUse,
        String titleToUse,
        String componentPrefix,
        std::function<void()> close,
        std::function<void(bool)> enabled) :
        owner (ownerToUse)
    ,   title (std::move(titleToUse)) {
    const String titleCase = title.substring(0, 1)
            + title.substring(1).toLowerCase();
    closeButton.setButtonText(String::fromUTF8("×"));
    closeButton.setComponentID(componentPrefix + ".close");
    closeButton.setTooltip("Close " + titleCase + " editor");
    closeButton.setWantsKeyboardFocus(true);
    closeButton.onClick = std::move(close);
    owner.addAndMakeVisible(closeButton);

    if (enabled) {
        enabledButton = std::make_unique<EffectEnableButton>();
        enabledButton->setComponentID(componentPrefix + ".enabled");
        enabledButton->onClick = [this, callback = std::move(enabled)] {
            callback(enabledButton->getToggleState());
        };
        owner.addAndMakeVisible(*enabledButton);
    }
}

void ExpandedEditorChrome::paint(Graphics& graphics) const {
    graphics.fillAll(Colour(0xff11151b));
    graphics.setColour(Colour(0xff2b3340));
    graphics.drawRoundedRectangle(
            owner.getLocalBounds().toFloat().reduced(0.5f),
            CanvasChromeMetrics::panelCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);
    graphics.setColour(Colour(0xffeef2f6));
    graphics.setFont(FontOptions(CanvasChromeMetrics::editorTitleFontSize));
    const auto header = fullEditorHeaderLayout(
            owner.getLocalBounds(), enabledButton != nullptr);
    graphics.drawText(title, header.title, Justification::centredLeft);
}

void ExpandedEditorChrome::resized() {
    const auto header = fullEditorHeaderLayout(
            owner.getLocalBounds(), enabledButton != nullptr);
    closeButton.setBounds(header.close);
    if (enabledButton != nullptr) {
        enabledButton->setBounds(header.enabled);
    }
}

void ExpandedEditorChrome::setEnabled(bool enabled) {
    if (enabledButton != nullptr) {
        enabledButton->setToggleState(enabled, dontSendNotification);
    }
}

bool ExpandedEditorChrome::isEnabled() const {
    return enabledButton == nullptr || enabledButton->getToggleState();
}

}
