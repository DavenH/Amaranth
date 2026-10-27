#include "UI/ReferenceTrayBar.h"

#include "UI/CanvasChromePalette.h"
#include "UI/ReferenceAssetBrowser.h"
#include "UI/LibraryIconRenderer.h"

namespace CycleV2 {

ReferenceTrayBar::ReferenceTrayBar() {
    setLookAndFeel(&lookAndFeel);
    LibraryIconRenderer::configure(disclosure, LibraryIcon::Tray, "Expand or collapse Wave References");
    disclosure.getProperties().set("leadingIcon", true);
    LibraryIconRenderer::configure(browse, LibraryIcon::Assets, "Browse audio assets");
    LibraryIconRenderer::configure(import, LibraryIcon::Import, "Import audio...");
    disclosure.setComponentID("comparison.toggle");
    browse.setComponentID("comparison.assets");
    import.setComponentID("comparison.import");
    disclosure.setTooltip("Expand or collapse wave references and pinned Spies");
    browse.setTooltip("Browse audio assets to audition or add a wave reference");
    for (auto* button : { &disclosure, &browse, &import }) {
        button->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button->setColour(juce::TextButton::buttonOnColourId, CanvasChromePalette::border);
        button->setColour(juce::TextButton::textColourOffId, CanvasChromePalette::text);
        addAndMakeVisible(*button);
    }
    disclosure.onClick = [this] {
        if (onToggle) {
            onToggle();
        }
    };
    browse.onClick = [this] {
        if (onBrowse) {
            onBrowse();
        }
    };
    import.onClick = [this] {
        if (onImport) {
            onImport();
        }
    };
    setExpanded(false);
}

ReferenceTrayBar::~ReferenceTrayBar() {
    setLookAndFeel(nullptr);
}

void ReferenceTrayBar::setExpanded(bool value) {
    expanded = value;
    disclosure.setToggleState(value, juce::dontSendNotification);
    repaint();
}

bool ReferenceTrayBar::isInterestedInDragSource(const SourceDetails& source) {
    return ReferenceAssetBrowser::referenceIdFromDrag(source.description).isNotEmpty();
}

void ReferenceTrayBar::itemDropped(const SourceDetails& source) {
    highlighted = false;
    repaint();
    const auto id = ReferenceAssetBrowser::referenceIdFromDrag(source.description);
    if (onDrop && id.isNotEmpty()) {
        onDrop(id);
    }
}

void ReferenceTrayBar::itemDragEnter(const SourceDetails&) {
    highlighted = true;
    repaint();
}

void ReferenceTrayBar::itemDragExit(const SourceDetails&) {
    highlighted = false;
    repaint();
}

std::vector<std::pair<juce::String, juce::Rectangle<float>>>
ReferenceTrayBar::pointerTargetsForAutomation() const {
    return {
            { disclosure.getComponentID(), disclosure.getBounds().toFloat() },
            { browse.getComponentID(), browse.getBounds().toFloat() },
            { import.getComponentID(), import.getBounds().toFloat() },
            { "comparison.drop", getLocalBounds().toFloat() }
    };
}

void ReferenceTrayBar::paint(juce::Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::dockSurface);
    graphics.setColour(highlighted ? CanvasChromePalette::navigationAccent
            : CanvasChromePalette::border);
    graphics.drawHorizontalLine(0, 0.f, (float) getWidth());
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(juce::FontOptions(12.f).withStyle("Bold"));
    graphics.drawText("Wave References", 44, 0, 116, getHeight(),
            juce::Justification::centredLeft);
    juce::Path chevron;
    const float y = (float) getHeight() * 0.5f;
    if (expanded) {
        chevron.startNewSubPath(170.f, y - 2.f);
        chevron.lineTo(174.f, y + 2.f);
        chevron.lineTo(178.f, y - 2.f);
    } else {
        chevron.startNewSubPath(172.f, y - 4.f);
        chevron.lineTo(176.f, y);
        chevron.lineTo(172.f, y + 4.f);
    }
    graphics.strokePath(chevron, juce::PathStrokeType(1.5f));
    if (highlighted) {
        graphics.drawRect(getLocalBounds(), 2);
    }
}

void ReferenceTrayBar::resized() {
    auto bounds = getLocalBounds().reduced(6, 2);
    disclosure.setBounds(bounds.removeFromLeft(185));
    import.setBounds(bounds.removeFromRight(32));
    bounds.removeFromRight(6);
    browse.setBounds(bounds.removeFromRight(32));
}

}
