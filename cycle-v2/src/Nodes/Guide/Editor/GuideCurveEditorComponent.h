#pragma once

#include "Nodes/Curve/Editor/CurveExpandedEditorComponent.h"

#include <memory>
#include <utility>
#include <vector>

namespace CycleV2 {

class GuideCurveEditorComponent final : public CurveExpandedEditorComponent {
public:
    explicit GuideCurveEditorComponent(CurveEditorWidget& widget);
    ~GuideCurveEditorComponent() override;

    static Rectangle<float> preferredHostBounds(Rectangle<float> canvasBounds);
    void setGuideResource(const GuideCurveResource& guide);
    void renderOpenGL(float scaleFactor) override;
    std::vector<std::pair<String, Rectangle<float>>> automationPointerTargets() const;

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
    GuideCurveResource guide;
};

}
