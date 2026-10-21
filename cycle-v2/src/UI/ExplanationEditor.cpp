#include "UI/ExplanationEditor.h"

#include "Graph/NodeGraph.h"
#include "UI/CanvasChromePalette.h"
#include "UI/NodePortLayout.h"
#include "UI/NodeViewModule.h"

namespace CycleV2 {

using namespace juce;

ExplanationEditor::ExplanationEditor(const String& title, const String& typeHelp,
        const String& currentText, SaveCallback saveCallback) :
        onSave(std::move(saveCallback)) {
    setComponentID("explanation.editor");
    setSize(430, typeHelp.isEmpty() ? 288 : 332);
    heading.setText(title, dontSendNotification);
    heading.setFont(FontOptions(16.f));
    heading.setColour(Label::textColourId, CanvasChromePalette::text);
    addAndMakeVisible(heading);
    help.setText(typeHelp, dontSendNotification);
    help.setFont(FontOptions(13.f));
    help.setColour(Label::textColourId, CanvasChromePalette::mutedText);
    help.setJustificationType(Justification::topLeft);
    addAndMakeVisible(help);
    text.setComponentID("explanation.text");
    text.setName("Explanation");
    text.setMultiLine(true, true);
    text.setReturnKeyStartsNewLine(true);
    text.setInputRestrictions(maximumExplanationLength);
    text.setFont(FontOptions(14.f));
    text.setIndents(9, 7);
    text.setText(currentText, false);
    text.setTextToShowWhenEmpty("Describe the sound, its role, and useful variations...",
            CanvasChromePalette::mutedText);
    text.setColour(TextEditor::backgroundColourId, CanvasChromePalette::restingControlSurface);
    text.setColour(TextEditor::textColourId, CanvasChromePalette::text);
    text.setColour(TextEditor::outlineColourId, CanvasChromePalette::border);
    text.setColour(TextEditor::focusedOutlineColourId, CanvasChromePalette::focus);
    text.setColour(TextEditor::highlightColourId, CanvasChromePalette::areaSelection);
    text.onEscapeKey = [this] { dismiss(); };
    addAndMakeVisible(text);
    save.setComponentID("explanation.save");
    cancel.setComponentID("explanation.cancel");
    save.onClick = [this] { submit(); };
    cancel.onClick = [this] { dismiss(); };
    for (auto* button : { &save, &cancel }) {
        button->setColour(TextButton::buttonColourId, CanvasChromePalette::restingControlSurface);
        button->setColour(TextButton::textColourOffId, CanvasChromePalette::text);
        addAndMakeVisible(*button);
    }
    resized();
}

void ExplanationEditor::focusText() {
    text.grabKeyboardFocus();
    text.moveCaretToEnd();
}

void ExplanationEditor::resized() {
    auto content = getLocalBounds().reduced(14);
    heading.setBounds(content.removeFromTop(26));
    if (help.getText().isNotEmpty()) {
        content.removeFromTop(6);
        help.setBounds(content.removeFromTop(38));
    }
    content.removeFromTop(10);
    auto buttons = content.removeFromBottom(28);
    save.setBounds(buttons.removeFromRight(72));
    buttons.removeFromRight(8);
    cancel.setBounds(buttons.removeFromRight(72));
    content.removeFromBottom(12);
    text.setBounds(content);
}

void ExplanationEditor::paint(Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::raisedSurface);
}

bool ExplanationEditor::keyPressed(const KeyPress& key) {
    if (key == KeyPress::escapeKey) {
        dismiss();
        return true;
    }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == KeyPress::returnKey) {
        submit();
        return true;
    }
    return false;
}

void ExplanationEditor::submit() {
    if (onSave && onSave(text.getText())) {
        dismiss();
    }
}

void ExplanationEditor::dismiss() {
    if (auto* callout = findParentComponentOfClass<CallOutBox>()) {
        callout->dismiss();
    }
}

var ExplanationEditor::automationState() const {
    auto* state = new DynamicObject();
    state->setProperty("text", text.getText());
    state->setProperty("title", heading.getText());
    state->setProperty("typeHelp", help.getText());
    return state;
}

bool ExplanationBadge::isVisible(const Node& node, const String& selectedNodeId) {
    return node.rationale.isNotEmpty() || node.id == selectedNodeId;
}

float ExplanationBadge::titleReservation(const Node& node, bool globalProcessing) {
    const auto& capabilities = NodeViewModuleRegistry::instance().moduleFor(node.kind).capabilities();
    const bool action = supportsOperationPortLayout(node) || supportsSinglePortLayout(node)
            || capabilities.outputSideControl;
    if (node.kind == NodeKind::Envelope) {
        return 108.f;
    }
    return globalProcessing ? (action ? 105.f : 65.f) : (action ? 65.f : 26.f);
}

Rectangle<float> ExplanationBadge::boundsFor(const Node& node,
        Rectangle<float> nodeBounds, float zoom, bool globalProcessing) {
    const float size = jmin(27.f * zoom, jmin(nodeBounds.getWidth(), nodeBounds.getHeight()) - 8.f * zoom);
    const float inset = size * 0.5f + 4.f * zoom;
    const float offset = titleReservation(node, globalProcessing) - 5.f;
    return Rectangle<float>(size, size).withCentre({
            jlimit(nodeBounds.getX() + inset, nodeBounds.getRight() - inset,
                    nodeBounds.getRight() - offset * zoom),
            jlimit(nodeBounds.getY() + inset, nodeBounds.getBottom() - inset,
                    nodeBounds.getY() + 21.f * zoom)
    });
}

void ExplanationBadge::paint(Graphics& graphics, Rectangle<float> bounds, bool authored) {
    bounds = bounds.reduced(bounds.getWidth() * 0.18f);
    graphics.setColour(CanvasChromePalette::mutedText.withAlpha(authored ? 0.85f : 0.55f));
    graphics.drawEllipse(bounds, jmax(0.6f, bounds.getWidth() * 0.05f));
    graphics.setFont(FontOptions(bounds.getHeight() * 0.78f));
    graphics.drawText("i", bounds, Justification::centred);
}

PresetExplanationButton::PresetExplanationButton() : Button("Preset explanation") {
    setTitle("Preset explanation");
    setDescription("Read or edit this preset's explanation");
    setWantsKeyboardFocus(true);
    setMouseCursor(MouseCursor::PointingHandCursor);
}

void PresetExplanationButton::paintButton(Graphics& graphics, bool highlighted, bool down) {
    const auto bounds = getLocalBounds().toFloat();
    if (highlighted || down || hasKeyboardFocus(false)) {
        graphics.setColour(CanvasChromePalette::hoveredControlSurface);
        graphics.fillEllipse(bounds.reduced(2.f));
    }
    ExplanationBadge::paint(graphics, bounds, true);
    if (hasKeyboardFocus(false)) {
        graphics.setColour(CanvasChromePalette::focus);
        graphics.drawEllipse(bounds.reduced(1.f), 1.f);
    }
}

}
