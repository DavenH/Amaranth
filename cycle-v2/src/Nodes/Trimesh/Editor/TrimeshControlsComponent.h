#pragma once

#include "Graph/NodeGraph.h"
#include "Nodes/Trimesh/Editor/TrimeshWidget.h"

#include <JuceHeader.h>

#include <memory>
#include <vector>

namespace CycleV2 {

class TrimeshControlsDelegate {
public:
    virtual ~TrimeshControlsDelegate() = default;
    virtual void setTrimeshPrimaryAxis(const juce::String& axis) = 0;
    virtual void toggleTrimeshLinkAxis(const juce::String& axis) = 0;
    virtual float trimeshMorphControlValue(const juce::String& id) const = 0;
    virtual void beginTrimeshMorphControlEdit(const juce::String& id, float value) = 0;
    virtual void updateTrimeshMorphControlEdit(float value) = 0;
    virtual void endTrimeshMorphControlEdit() = 0;
    virtual void beginTrimeshOutputScaleControlEdit(const juce::String& id, float value) = 0;
    virtual void updateTrimeshOutputScaleControlEdit(float value) = 0;
    virtual void endTrimeshOutputScaleControlEdit() = 0;
    virtual void beginTrimeshVertexControlEdit(const juce::String& id, float value) = 0;
    virtual void updateTrimeshVertexControlEdit(float value) = 0;
    virtual void endTrimeshVertexControlEdit() = 0;
    virtual void showTrimeshVertexGuideMenu(
            const juce::String& id,
            juce::Rectangle<int> screenArea) = 0;
    virtual void selectTrimeshVertex(int index) = 0;
};

class TrimeshControlsComponent : public juce::Component {
public:
    explicit TrimeshControlsComponent(TrimeshWidget& widget);
    ~TrimeshControlsComponent() override;

    void setDelegate(TrimeshControlsDelegate* nextDelegate);
    void setNode(const Node& nextNode);
    void setContentBounds(juce::Rectangle<float> nextContentBounds);
    void refreshHitRegions();
    void refreshSelectionState();
    int getControlRegionCount() const { return static_cast<int>(controlRegions.size()); }
    int getMorphSliderCount() const;
    int getOutputScaleSliderCount() const;
    int getPrimaryAxisButtonCount() const;
    int getLinkToggleButtonCount() const;
    int getVertexParameterSliderCount() const;
    int getVertexGuideGainKnobCount() const;
    int getVertexGuideAttachmentButtonCount() const;
    juce::String focusedMorphAxis() const;

    juce::MouseCursor cursorFor(juce::Point<float> position);
    void beginPointerInteraction(
            juce::Point<float> position,
            juce::Rectangle<int> screenArea);
    void continuePointerInteraction(juce::Point<float> position);
    void endPointerInteraction();

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    class ControlTarget;

    enum class DragTarget {
        None,
        Morph,
        OutputScale,
        VertexParameter,
        VertexGuideGain
    };

    void updateHitRegions();
    int countControlRegions(TrimeshExpandedHitRegionKind kind) const;
    const TrimeshExpandedHitRegion* findControlRegion(juce::Point<float> position) const;
    void beginControlDrag(
            const TrimeshExpandedHitRegion& region,
            juce::Point<float> position,
            juce::Rectangle<int> screenArea);
    void dragControl(juce::Point<float> position);
    void endControlDrag();
    bool adjustGuideGainFromKeyboard(
            const TrimeshExpandedHitRegion& region,
            float amount);
    void rememberMorphFocus(const TrimeshExpandedHitRegion& region);
    bool selectMorphControl(const juce::String& axis);
    bool selectAdjacentMorphControl(int direction);
    bool setFocusedMorphTo(float value);
    bool adjustFocusedMorphBy(float amount);
    bool toggleFocusedMorphLink();
    bool isControlEnabled(const TrimeshExpandedHitRegion& region) const;
    void updateControlEnabledStates();

    TrimeshWidget& widget;
    TrimeshControlsDelegate* delegate {};
    Node node;
    DragTarget dragTarget { DragTarget::None };
    juce::String keyboardMorphAxis;
    juce::String activeParameterId;
    float guideGainDragStartY {};
    float guideGainDragStartValue {};
    juce::Rectangle<float> contentBounds;
    juce::Rectangle<int> lastHitRegionContentBounds;
    std::vector<TrimeshExpandedHitRegion> controlHitRegions;
    std::vector<std::unique_ptr<ControlTarget>> controlRegions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrimeshControlsComponent)
};

}
