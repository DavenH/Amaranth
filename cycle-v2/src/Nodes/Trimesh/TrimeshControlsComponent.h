#pragma once

#include "../../Graph/NodeGraph.h"
#include "TrimeshWidget.h"

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

namespace CycleV2 {

class TrimeshControlsComponent : public juce::Component {
public:
    struct Callbacks {
        std::function<void(const juce::String&)> setPrimaryAxis;
        std::function<void(const juce::String&, float)> beginMorphEdit;
        std::function<void(float)> updateMorphEdit;
        std::function<void()> endMorphEdit;
        std::function<void(const juce::String&, float)> beginVertexParameterEdit;
        std::function<void(float)> updateVertexParameterEdit;
        std::function<void()> endVertexParameterEdit;
    };

    explicit TrimeshControlsComponent(TrimeshWidget& widget);
    ~TrimeshControlsComponent() override;

    void setCallbacks(Callbacks nextCallbacks);
    void setNode(const Node& nextNode);
    void setContentBounds(juce::Rectangle<float> nextContentBounds);
    int getControlRegionCount() const { return (int) controlRegions.size(); }
    int getMorphSliderCount() const;
    int getPrimaryAxisButtonCount() const;

    void resized() override;

private:
    class DragRegionComponent;
    class MorphSlider;
    class PrimaryAxisButton;

    enum class DragTarget {
        None,
        Morph,
        VertexParameter
    };

    void updateHitRegions();
    void beginControlDrag(const TrimeshExpandedHitRegion& region, juce::Point<float> position);
    void dragControl(const TrimeshExpandedHitRegion& region, juce::Point<float> position);
    void endControlDrag();

    TrimeshWidget& widget;
    Callbacks callbacks;
    Node node;
    DragTarget dragTarget { DragTarget::None };
    juce::String activeParameterId;
    juce::Rectangle<float> contentBounds;
    juce::Rectangle<int> lastHitRegionContentBounds;
    std::vector<std::unique_ptr<juce::Component>> controlRegions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrimeshControlsComponent)
};

}
