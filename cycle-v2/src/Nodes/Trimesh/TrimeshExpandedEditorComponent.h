#pragma once

#include "../../Graph/NodeGraph.h"
#include "TrimeshWidget.h"

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

namespace CycleV2 {

class TrimeshExpandedEditorComponent : public juce::Component {
public:
    struct Callbacks {
        std::function<void()> close;
        std::function<void()> repaintOpenGL;
        std::function<void(const juce::String&)> setPrimaryAxis;
        std::function<void(const juce::String&, float)> beginMorphEdit;
        std::function<void(float)> updateMorphEdit;
        std::function<void()> endMorphEdit;
        std::function<void(const juce::String&, float)> beginVertexParameterEdit;
        std::function<void(float)> updateVertexParameterEdit;
        std::function<void()> endVertexParameterEdit;
        std::function<void(int)> selectVertex;
    };

    explicit TrimeshExpandedEditorComponent(TrimeshWidget& widget);
    ~TrimeshExpandedEditorComponent() override;

    void setCallbacks(Callbacks nextCallbacks);
    void setNode(const Node& nextNode);
    void renderOpenGL(float scaleFactor);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    class HitRegionComponent;

    enum class DragTarget {
        None,
        Morph,
        VertexParameter
    };

    juce::Rectangle<float> closeButtonBounds() const;
    juce::Rectangle<float> contentBounds() const;
    juce::MouseCursor cursorFor(juce::Point<float> position);
    void updateCursor(juce::Point<float> position);
    void updatePanelHosts();
    void updateHitRegions();
    void beginControlDrag(const TrimeshExpandedHitRegion& region, juce::Point<float> position);
    void dragControl(const TrimeshExpandedHitRegion& region, juce::Point<float> position);
    void endControlDrag();

    TrimeshWidget& widget;
    Callbacks callbacks;
    Node node;
    DragTarget dragTarget { DragTarget::None };
    juce::String activeParameterId;
    juce::Rectangle<int> lastHitRegionContentBounds;
    std::vector<std::unique_ptr<HitRegionComponent>> hitRegions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrimeshExpandedEditorComponent)
};

}
