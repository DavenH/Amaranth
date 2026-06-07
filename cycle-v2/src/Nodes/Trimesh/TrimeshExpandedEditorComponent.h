#pragma once

#include "../../Graph/NodeGraph.h"
#include "TrimeshControlsComponent.h"
#include "TrimeshWidget.h"

#include <JuceHeader.h>

#include <array>
#include <functional>
#include <memory>

namespace CycleV2 {

class TrimeshExpandedEditorComponent : public juce::Component {
public:
    struct Callbacks {
        std::function<void()> close;
        std::function<void()> repaintOpenGL;
        std::function<void(const juce::String&)> setPrimaryAxis;
        std::function<void(const juce::String&)> toggleLinkAxis;
        std::function<void(const juce::String&, float)> beginMorphEdit;
        std::function<void(float)> updateMorphEdit;
        std::function<void()> endMorphEdit;
        std::function<void(const juce::String&, float)> beginVertexParameterEdit;
        std::function<void(float)> updateVertexParameterEdit;
        std::function<void()> endVertexParameterEdit;
        std::function<void(const juce::String&, juce::Rectangle<int>)> showVertexGuideAttachmentMenu;
        std::function<void(int)> selectVertex;
    };

    explicit TrimeshExpandedEditorComponent(TrimeshWidget& widget);
    ~TrimeshExpandedEditorComponent() override;

    void setCallbacks(Callbacks nextCallbacks);
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
    enum class DragTarget {
        None,
        Morph,
        VertexParameter
    };

    juce::Rectangle<float> closeButtonBounds() const;
    juce::Rectangle<float> contentBounds() const;
    juce::String vertexGuideParameterField(const juce::String& parameterId) const;
    juce::MouseCursor cursorFor(juce::Point<float> position);
    void updateCursor(juce::Point<float> position);
    void updatePanelHosts();
    void updateControlsHost();

    TrimeshWidget& widget;
    Callbacks callbacks;
    TrimeshControlsComponent controls;
    Node node;
    TrimeshRenderProfile renderProfile { TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal) };
    DragTarget dragTarget { DragTarget::None };
    juce::String activeParameterId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrimeshExpandedEditorComponent)
};

}
