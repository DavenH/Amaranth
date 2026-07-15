#pragma once

#include "Effect2DExpandedEditorComponent.h"

#include <memory>

namespace CycleV2 {

class WaveshaperEditorComponent final : public Effect2DExpandedEditorComponent {
public:
    explicit WaveshaperEditorComponent(Effect2DWidget& widget);
    ~WaveshaperEditorComponent() override;

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

class ImpulseResponseEditorComponent final : public Effect2DExpandedEditorComponent {
public:
    explicit ImpulseResponseEditorComponent(Effect2DWidget& widget);
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

class GuideCurveEditorComponent final : public Effect2DExpandedEditorComponent {
public:
    explicit GuideCurveEditorComponent(Effect2DWidget& widget);
    ~GuideCurveEditorComponent() override;

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

class EnvelopeEditorComponent final : public Effect2DExpandedEditorComponent {
public:
    explicit EnvelopeEditorComponent(Effect2DWidget& widget);
    ~EnvelopeEditorComponent() override;

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

    std::unique_ptr<Impl> impl;
};

std::unique_ptr<Effect2DExpandedEditorComponent> createCurveNodeEditor(
        NodeKind kind,
        Effect2DWidget& widget);

}
