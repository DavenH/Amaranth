#pragma once

#include "Nodes/Curve/Editor/CurveExpandedEditorComponent.h"

#include <memory>

namespace CycleV2 {

class PrecisionSlider;

class EnvelopeEditorComponent final : public CurveExpandedEditorComponent,
                                      public TooltipClient {
public:
    explicit EnvelopeEditorComponent(CurveEditorWidget& target);
    ~EnvelopeEditorComponent() override;

    void renderOpenGL(float scaleFactor) override;
    String getTooltip() override;
    bool keyPressed(const juce::KeyPress& key) override;
    juce::String focusedMorphAxis() const;

private:
    struct Impl;
    Rectangle<float> morphSurfaceBounds() const;
    Rectangle<float> editorPanelBounds() const override;
    Rectangle<float> editorControlBounds() const override;
    MorphPosition displayedMorphPosition() const;
    void bindMorphControl(int axis);
    void beginMorphGesture();
    void cancelMorphGesture();
    bool setLocalMorphValue(int axis, float value);
    void paintEditor(Graphics&) override;
    void layoutEditor() override;
    void syncEditorFromNode() override;
    void applyEditorStateToWidget() override;
    bool editorInteractionIsSessionOnly(Point<float> position) const override;
    std::vector<NodeParameter> editorControls() const override;
    void appendEditorAutomation(DynamicObject&) const override;
    bool editorMouseMove(Point<float>) override;
    bool updatePropertyFeedback(Point<float> position);
    bool editorMouseDown(Point<float>) override;
    bool editorMouseDrag(Point<float>) override;
    void editorMouseUp() override;
    void syncInteractionControls() override;
    bool handleAxisMouseDown(Point<float> position, Rectangle<float> controls);
    bool handleVertexParameterMouseDown(Point<float> position, Rectangle<float> controls);
    bool dragMorph(Point<float> position);
    bool dragVertexParameter(Point<float> position);
    bool handleMorphKeyboardShortcut(const juce::KeyPress& key);
    void rememberMorphFocus(const juce::String& axis);
    bool selectableMorphAxis(const juce::String& axis) const;
    bool selectMorphAxis(const juce::String& axis);
    bool selectAdjacentMorphAxis(int direction);
    PrecisionSlider* focusedMorphSlider() const;
    void updateKeyboardShortcutHint();

    std::unique_ptr<Impl> impl;
};

}
