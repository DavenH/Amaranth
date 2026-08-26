#pragma once

#include "Nodes/Curve/Editor/CurveExpandedEditorComponent.h"

#include <memory>

namespace CycleV2 {

class ImpulseResponseEditorComponent final : public CurveExpandedEditorComponent {
public:
    explicit ImpulseResponseEditorComponent(CurveEditorWidget& widget);
    ~ImpulseResponseEditorComponent() override;

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

    std::unique_ptr<Impl> impl;
};

}
