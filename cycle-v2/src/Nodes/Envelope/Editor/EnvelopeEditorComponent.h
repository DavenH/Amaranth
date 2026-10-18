#pragma once

#include "Nodes/Curve/Editor/CurveExpandedEditorComponent.h"

#include <memory>

namespace CycleV2 {

class PrecisionSlider;

class EnvelopeEditorComponent final : public CurveExpandedEditorComponent,
                                      public TooltipClient,
                                      private juce::KeyListener {
public:
    explicit EnvelopeEditorComponent(CurveEditorWidget& target);
    ~EnvelopeEditorComponent() override;

    String getTooltip() override;
    bool keyPressed(const juce::KeyPress& key) override;
    juce::String focusedMorphAxis() const;

private:
    struct Impl;
    Rectangle<float> editorPanelBounds() const override;
    Rectangle<float> editorControlBounds() const override;
    void paintEditor(Graphics&) override;
    void layoutEditor() override;
    void focusOfChildComponentChanged(juce::Component::FocusChangeType cause) override;
    void syncEditorFromNode() override;
    void applyEditorStateToWidget() override;
    bool editorInteractionIsSessionOnly(Point<float> position) const override;
    std::vector<NodeParameter> editorControls() const override;
    void appendEditorAutomation(DynamicObject&) const override;
    bool editorMouseMove(Point<float>) override;
    bool editorMouseDown(Point<float>) override;
    bool editorMouseDrag(Point<float>) override;
    void editorMouseUp() override;
    void syncInteractionControls() override;
    bool handleAxisMouseDown(Point<float> position, Rectangle<float> controls);
    bool handleVertexParameterMouseDown(Point<float> position, Rectangle<float> controls);
    bool dragMorph(Point<float> position);
    bool dragVertexParameter(Point<float> position);
    bool keyPressed(const juce::KeyPress& key, juce::Component* source) override;
    bool handleMorphKeyboardShortcut(const juce::KeyPress& key);
    bool focusMorphAxis(const juce::String& axis);
    PrecisionSlider* focusedMorphSlider() const;

    std::unique_ptr<Impl> impl;
};

}
