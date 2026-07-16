#pragma once

#include "../../Graph/NodeGraph.h"
#include "TrimeshWidget.h"

#include <JuceHeader.h>

#include <memory>
#include <vector>

namespace CycleV2 {

class TrimeshControlsDelegate {
public:
    virtual ~TrimeshControlsDelegate() = default;
    virtual void setTrimeshPrimaryAxis(const juce::String& axis) = 0;
    virtual void toggleTrimeshLinkAxis(const juce::String& axis) = 0;
    virtual void beginTrimeshMorphControlEdit(const juce::String& id, float value) = 0;
    virtual void updateTrimeshMorphControlEdit(float value) = 0;
    virtual void endTrimeshMorphControlEdit() = 0;
    virtual void beginTrimeshVertexControlEdit(const juce::String& id, float value) = 0;
    virtual void updateTrimeshVertexControlEdit(float value) = 0;
    virtual void endTrimeshVertexControlEdit() = 0;
    virtual void showTrimeshVertexGuideMenu(
            const juce::String& id,
            juce::Rectangle<int> screenArea) = 0;
};

class TrimeshControlsComponent : public juce::Component {
public:
    explicit TrimeshControlsComponent(TrimeshWidget& widget);
    ~TrimeshControlsComponent() override;

    void setDelegate(TrimeshControlsDelegate* nextDelegate);
    void setNode(const Node& nextNode);
    void setContentBounds(juce::Rectangle<float> nextContentBounds);
    int getControlRegionCount() const { return (int) controlRegions.size(); }
    int getMorphSliderCount() const;
    int getPrimaryAxisButtonCount() const;
    int getLinkToggleButtonCount() const;
    int getVertexParameterSliderCount() const;
    int getVertexGuideAttachmentButtonCount() const;

    void resized() override;

private:
    class ControlSlider;
    class PrimaryAxisButton;

    enum class DragTarget {
        None,
        Morph,
        VertexParameter
    };

    void updateHitRegions();
    void beginControlDrag(
            const TrimeshExpandedHitRegion& region,
            juce::Point<float> position,
            juce::Rectangle<int> screenArea);
    void dragControl(const TrimeshExpandedHitRegion& region, juce::Point<float> position);
    void endControlDrag();

    TrimeshWidget& widget;
    TrimeshControlsDelegate* delegate {};
    Node node;
    DragTarget dragTarget { DragTarget::None };
    juce::String activeParameterId;
    juce::Rectangle<float> contentBounds;
    juce::Rectangle<int> lastHitRegionContentBounds;
    std::vector<std::unique_ptr<juce::Component>> controlRegions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrimeshControlsComponent)
};

}
