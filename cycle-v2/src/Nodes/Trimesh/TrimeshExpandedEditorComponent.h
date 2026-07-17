#pragma once

#include "../../Graph/NodeGraph.h"
#include "TrimeshControlsComponent.h"
#include "TrimeshWidget.h"

#include <JuceHeader.h>

#include <array>
#include <memory>

namespace CycleV2 {

class TrimeshExpandedEditorDelegate {
public:
    virtual ~TrimeshExpandedEditorDelegate() = default;
    virtual void closeTrimeshEditor() = 0;
    virtual void repaintTrimeshEditorOpenGL() = 0;
    virtual void setTrimeshPrimaryAxisValue(const juce::String& axis) = 0;
    virtual void toggleTrimeshLinkAxisValue(const juce::String& axis) = 0;
    virtual void beginTrimeshMorphEdit(const juce::String& id, float value) = 0;
    virtual void updateTrimeshMorphEdit(float value) = 0;
    virtual void endTrimeshMorphEdit() = 0;
    virtual void beginTrimeshVertexParameterEdit(const juce::String& id, float value) = 0;
    virtual void updateTrimeshVertexParameterEdit(float value) = 0;
    virtual void endTrimeshVertexParameterEdit() = 0;
    virtual void showTrimeshGuideAttachmentMenu(
            const juce::String& field,
            juce::Rectangle<int> area) = 0;
    virtual void selectTrimeshVertex(int index) = 0;
};

class TrimeshExpandedEditorComponent : public juce::Component,
                                       private TrimeshControlsDelegate {
public:
    explicit TrimeshExpandedEditorComponent(TrimeshWidget& widget);
    ~TrimeshExpandedEditorComponent() override;

    void setDelegate(TrimeshExpandedEditorDelegate* nextDelegate);
    void setNode(const Node& nextNode);
    void setGuideAttachmentLabels(std::array<juce::String, 6> labels);
    void setDisplayDomain(PortDomain domain);
    void setRenderProfile(TrimeshRenderProfile profile);
    void renderOpenGL(float scaleFactor);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    juce::Rectangle<float> closeButtonBounds() const;
    juce::Rectangle<float> contentBounds() const;
    juce::String vertexGuideParameterField(const juce::String& parameterId) const;
    juce::MouseCursor cursorFor(juce::Point<float> position);
    void updateCursor(juce::Point<float> position);
    void updatePanelHosts();
    void updateControlsHost();
    void setTrimeshPrimaryAxis(const juce::String& axis) override;
    void toggleTrimeshLinkAxis(const juce::String& axis) override;
    void beginTrimeshMorphControlEdit(const juce::String& id, float value) override;
    void updateTrimeshMorphControlEdit(float value) override;
    void endTrimeshMorphControlEdit() override;
    void beginTrimeshVertexControlEdit(const juce::String& id, float value) override;
    void updateTrimeshVertexControlEdit(float value) override;
    void endTrimeshVertexControlEdit() override;
    void showTrimeshVertexGuideMenu(
            const juce::String& id,
            juce::Rectangle<int> screenArea) override;
    void selectTrimeshVertex(int index) override;

    TrimeshWidget& widget;
    TrimeshExpandedEditorDelegate* delegate {};
    TrimeshControlsComponent controls;
    Node node;
    TrimeshRenderProfile renderProfile { TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrimeshExpandedEditorComponent)
};

}
