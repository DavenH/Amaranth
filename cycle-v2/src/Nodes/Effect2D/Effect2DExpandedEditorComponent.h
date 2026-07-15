#pragma once

#include "../../Graph/NodeGraph.h"
#include "Effect2DWidget.h"

#include <JuceHeader.h>

#include <functional>
#include <vector>

namespace CycleV2 {

class Effect2DExpandedEditorComponent : public Component {
public:
    struct Callbacks {
        std::function<void()> close;
        std::function<void()> repaintOpenGL;
        std::function<bool(const String&, uint64_t, const std::vector<NodeParameter>&)> publishState;
        std::function<void()> beginTransaction;
        std::function<void()> commitTransaction;
    };

    explicit Effect2DExpandedEditorComponent(Effect2DWidget& widget);
    ~Effect2DExpandedEditorComponent() override;

    void setCallbacks(Callbacks nextCallbacks);
    void setNode(const Node& nextNode);
    void renderOpenGL(float scaleFactor);
    Rectangle<float> panelBoundsForAutomation() const;
    var automationState() const;

    void paint(Graphics& graphics) override;
    void resized() override;
    void mouseMove(const MouseEvent& event) override;
    void mouseDown(const MouseEvent& event) override;
    void mouseDrag(const MouseEvent& event) override;
    void mouseUp(const MouseEvent& event) override;

protected:
    virtual Rectangle<float> editorPanelBounds() const = 0;
    virtual Rectangle<float> editorControlBounds() const = 0;
    virtual void paintEditor(Graphics& graphics) = 0;
    virtual void layoutEditor() = 0;
    virtual void syncEditorFromNode() = 0;
    virtual void applyEditorStateToWidget() = 0;
    virtual std::vector<NodeParameter> editorControls() const = 0;
    virtual void appendEditorAutomation(DynamicObject& state) const = 0;
    virtual bool editorMouseMove(Point<float> position);
    virtual bool editorMouseDown(Point<float> position);
    virtual bool editorMouseDrag(Point<float> position);
    virtual void editorMouseUp();

    Rectangle<float> contentBounds() const;
    void publishCurrentState();
    void beginTransaction();
    void commitTransaction();
    void requestRepaint();

    Effect2DWidget& widget;
    Node node;
    Callbacks callbacks;
    bool syncingControls {};

private:
    Rectangle<float> closeButtonBounds() const;
    void updatePanelHost();
    void persistEffectMeshState();

    bool transactionActive {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Effect2DExpandedEditorComponent)
};

}
