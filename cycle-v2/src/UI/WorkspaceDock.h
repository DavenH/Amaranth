#pragma once

#include <JuceHeader.h>

#include <vector>

namespace CycleV2 {

struct WorkspaceDockState {
    bool expanded { true };
    bool leftMinimized {};
    bool rightMinimized {};
    float expandedHeight { 190.f };
};

struct WorkspaceDockLayout {
    juce::Rectangle<float> workspace;
    juce::Rectangle<float> content;
    juce::Rectangle<float> dock;
    juce::Rectangle<float> leftShelf;
    juce::Rectangle<float> rightShelf;
    juce::Rectangle<float> resizeHandle;
    juce::Rectangle<float> collapseHandle;
};

struct WorkspaceDockSpyControls {
    juce::Rectangle<float> label;
    juce::Rectangle<float> refresh;
    juce::Rectangle<float> minimize;
};

enum class WorkspaceDockIcon {
    Add,
    ChevronLeft,
    ChevronRight
};

enum class WorkspaceDockFocusTarget {
    None,
    Collapse,
    GuideDrawer,
    GuideMinimize,
    GuideAdd,
    GuideTile,
    SpyDrawer,
    SpyMinimize,
    SpyRefresh,
    SpyTile
};

struct WorkspaceDockFocus {
    WorkspaceDockFocusTarget target { WorkspaceDockFocusTarget::None };
    juce::String itemId;

    bool operator==(const WorkspaceDockFocus& other) const {
        return target == other.target && itemId == other.itemId;
    }

    bool operator!=(const WorkspaceDockFocus& other) const {
        return !(*this == other);
    }
};

class WorkspaceDock {
public:
    static constexpr float collapsedHeight = 34.f;
    static constexpr float minimumExpandedHeight = 120.f;
    static constexpr float drawerWidth = 36.f;
    static constexpr float shelfPadding = 10.f;
    static constexpr float headerHeight = 36.f;
    static constexpr float tileWidth = 190.f;
    static constexpr float guideTileHeight = 92.f;
    static constexpr float tileGap = 8.f;
    static constexpr float tileBottomPadding = 8.f;
    static constexpr float controlSize = 26.f;
    static constexpr float addButtonWidth = 48.f;

    static WorkspaceDockLayout layout(
            juce::Rectangle<float> workspace,
            const WorkspaceDockState& state);
    static juce::Rectangle<float> editorAvailableBounds(const WorkspaceDockLayout& layout);
    static WorkspaceDockSpyControls spyControls(juce::Rectangle<float> rail);
    static juce::Rectangle<float> spyRowBounds(
            juce::Rectangle<float> workspace,
            bool expanded,
            float expandedHeight);
    static juce::Rectangle<float> headerBounds(juce::Rectangle<float> shelf);
    static juce::Rectangle<float> tileBounds(
            juce::Rectangle<float> shelf,
            int tileIndex,
            float horizontalOffset);
    static juce::Rectangle<float> guideTileBounds(
            juce::Rectangle<float> shelf,
            int tileIndex,
            float verticalOffset);
    static float offsetToRevealGuideTile(
            float currentOffset,
            float maximumOffset,
            float shelfHeight,
            int tileIndex);
    static void paintVerticalOverflowFeedback(
            juce::Graphics& graphics,
            juce::Rectangle<float> shelf,
            float verticalOffset,
            float maximumOffset);
    static juce::Rectangle<float> vacancyBounds(juce::Rectangle<float> shelf);
    static WorkspaceDockFocus advanceFocus(
            const std::vector<WorkspaceDockFocus>& order,
            const WorkspaceDockFocus& current,
            int direction);
    static float offsetToRevealTile(
            float currentOffset,
            float maximumOffset,
            float shelfWidth,
            int tileIndex);
    static void paintIconButton(
            juce::Graphics& graphics,
            juce::Rectangle<float> bounds,
            WorkspaceDockIcon icon,
            bool focused);
    static void paintTileChrome(
            juce::Graphics& graphics,
            juce::Rectangle<float> tile,
            bool selected,
            bool hovered,
            bool focused);
    static void paintOverflowFeedback(
            juce::Graphics& graphics,
            juce::Rectangle<float> shelf,
            float horizontalOffset,
            float maximumHorizontalOffset);
    static void paintChrome(
            juce::Graphics& graphics,
            const WorkspaceDockLayout& layout,
            const juce::String& leftSummary,
            const juce::String& rightSummary,
            bool expanded,
            bool focused);
};

}
