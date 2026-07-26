#pragma once

#include "../../Graph/NodeGraph.h"
#include "Effect2DWidget.h"

#include <JuceHeader.h>

#include <initializer_list>
#include <utility>
#include <vector>

namespace CycleV2 {

class LabeledParameterSlider;
class ParameterToggle;

class CurveExpandedEditorDelegate {
public:
    virtual ~CurveExpandedEditorDelegate() = default;
    virtual void closeEffect2DEditor() = 0;
    virtual void repaintEffect2DEditorOpenGL() = 0;
    virtual bool publishEffect2DState(
            NodeModelStatePtr model,
            const std::vector<NodeParameter>& controls) = 0;
    virtual void beginEffect2DTransaction() = 0;
    virtual void commitEffect2DTransaction() = 0;
    virtual void effect2DTransientStateChanged(uint64_t) {}
};

class CurveExpandedEditorComponent : public Component,
                                     private CurvePanelControllerDelegate {
public:
    explicit CurveExpandedEditorComponent(Effect2DWidget& widget);
    ~CurveExpandedEditorComponent() override;

    void setDelegate(CurveExpandedEditorDelegate* nextDelegate);
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
    void bindContinuousControl(LabeledParameterSlider& control);
    void bindContinuousControls(std::initializer_list<LabeledParameterSlider*> controls);
    void bindDiscreteControl(ParameterToggle& control);
    void bindDiscreteControl(ComboBox& control);

    template<typename Operation>
    void bindDiscreteAction(Button& button, Operation operation) {
        button.onClick = [this, operation = std::move(operation)] {
            performDiscreteEdit(operation);
        };
    }

    Effect2DWidget& widget;
    Node node;
    CurveExpandedEditorDelegate* delegate {};
    bool syncingControls {};

private:
    template<typename Operation>
    void performDiscreteEdit(Operation& operation) {
        beginTransaction();
        operation();
        publishCurrentState();
        commitTransaction();
    }

    Rectangle<float> closeButtonBounds() const;
    void updatePanelHost();
    void persistEffectMeshState();
    bool publishModelState();
    void repaintCurvePanelController() override;
    void setCurvePanelControllerCursor(const MouseCursor& cursor) override;
    void beginCurvePanelControllerEdit() override;
    void curvePanelControllerEdited() override;
    void commitCurvePanelControllerEdit() override;

    bool transactionActive {};
    bool transientStateChanged {};
    uint64_t transactionBaseRevision {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurveExpandedEditorComponent)
};

}
