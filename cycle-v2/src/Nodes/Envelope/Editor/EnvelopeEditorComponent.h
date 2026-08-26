#pragma once

#include "Nodes/Curve/Editor/CurveExpandedEditorComponent.h"

#include <memory>

namespace CycleV2 {

class EnvelopeEditorComponent final : public CurveExpandedEditorComponent,
                                      public TooltipClient {
public:
    explicit EnvelopeEditorComponent(CurveEditorWidget& widget);
    ~EnvelopeEditorComponent() override;

    String getTooltip() override;

private:
    struct Impl;
    Rectangle<float> editorPanelBounds() const override;
    Rectangle<float> editorControlBounds() const override;
    void paintEditor(Graphics&) override;
    void layoutEditor() override;
    void syncEditorFromNode() override;
    void applyEditorStateToWidget() override;
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

    std::unique_ptr<Impl> impl;
};

}
