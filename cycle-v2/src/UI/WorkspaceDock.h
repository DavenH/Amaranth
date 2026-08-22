#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

struct WorkspaceDockState {
    bool expanded { true };
    bool leftMinimized {};
    bool rightMinimized {};
    float expandedHeight { 190.f };
    float splitRatio { 0.5f };
};

struct WorkspaceDockLayout {
    juce::Rectangle<float> workspace;
    juce::Rectangle<float> content;
    juce::Rectangle<float> dock;
    juce::Rectangle<float> leftShelf;
    juce::Rectangle<float> rightShelf;
    juce::Rectangle<float> divider;
    juce::Rectangle<float> resizeHandle;
    juce::Rectangle<float> collapseHandle;
};

class WorkspaceDock {
public:
    static constexpr float collapsedHeight = 28.f;
    static constexpr float minimumExpandedHeight = 120.f;
    static constexpr float minimumShelfWidth = 240.f;
    static constexpr float drawerWidth = 30.f;

    static WorkspaceDockLayout layout(
            juce::Rectangle<float> workspace,
            const WorkspaceDockState& state);
    static float clampedSplitRatio(
            juce::Rectangle<float> workspace,
            float splitRatio);
    static void paintChrome(
            juce::Graphics& graphics,
            const WorkspaceDockLayout& layout,
            const juce::String& leftSummary,
            const juce::String& rightSummary,
            bool expanded);
};

}
