#include <utility>

#include "UI/NodeGroupRenameCallout.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

NodeGroupRenameCallout::NodeGroupRenameCallout(
        const juce::String& currentName,
        RenameCallback rename) :
        onRename(std::move(rename)) {
    setSize(260, 54);
    name.setName("Group name");
    name.setText(currentName, false);
    name.setTextToShowWhenEmpty("Group name", CanvasChromePalette::mutedText);
    name.setFont(juce::FontOptions(15.f));
    name.setIndents(10, 5);
    name.setColour(juce::TextEditor::backgroundColourId,
            CanvasChromePalette::restingControlSurface);
    name.setColour(juce::TextEditor::outlineColourId,
            CanvasChromePalette::border);
    name.setColour(juce::TextEditor::focusedOutlineColourId,
            CanvasChromePalette::focus);
    name.setColour(juce::TextEditor::textColourId,
            CanvasChromePalette::text);
    name.setColour(juce::TextEditor::highlightColourId,
            CanvasChromePalette::areaSelection);
    name.onReturnKey = [this] { submit(); };
    name.onEscapeKey = [this] { dismiss(); };
    name.onTextChange = [this] {
        name.setColour(juce::TextEditor::focusedOutlineColourId,
                CanvasChromePalette::focus);
    };
    addAndMakeVisible(name);
}

void NodeGroupRenameCallout::focusName() {
    name.grabKeyboardFocus();
    name.selectAll();
}

void NodeGroupRenameCallout::resized() {
    name.setBounds(getLocalBounds().reduced(10, 10));
}

void NodeGroupRenameCallout::drawCallOutBoxBackground(
        juce::CallOutBox&,
        juce::Graphics& graphics,
        const juce::Path& outline,
        juce::Image&) {
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillPath(outline);
    graphics.setColour(CanvasChromePalette::strongBorder.withAlpha(0.7f));
    graphics.strokePath(outline, juce::PathStrokeType(1.f));
}

int NodeGroupRenameCallout::getCallOutBoxBorderSize(const juce::CallOutBox&) {
    return 8;
}

float NodeGroupRenameCallout::getCallOutBoxCornerSize(const juce::CallOutBox&) {
    return 4.f;
}

void NodeGroupRenameCallout::submit() {
    const auto title = name.getText().trim();
    if (title.isEmpty()) {
        name.setColour(juce::TextEditor::focusedOutlineColourId,
                CanvasChromePalette::destructive);
        return;
    }
    if (onRename) {
        onRename(title);
    }
    dismiss();
}

void NodeGroupRenameCallout::dismiss() {
    if (auto* callout = findParentComponentOfClass<juce::CallOutBox>()) {
        callout->dismiss();
    }
}

}
