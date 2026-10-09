#pragma once

#include <JuceHeader.h>

#include <vector>

namespace CycleV2 {

struct WorkspaceDockState {
    bool leftMinimized {};
};

struct WorkspaceDockLayout {
    juce::Rectangle<float> workspace;
    juce::Rectangle<float> content;
    juce::Rectangle<float> leftShelf;
};

enum class WorkspaceDockIcon {
    Add,
    ChevronLeft,
    ChevronRight
};

enum class WorkspaceDockFocusTarget {
    None,
    GuideDrawer,
    GuideMinimize,
    GuideAdd,
    GuideTile
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
    static constexpr float drawerWidth = 36.f;
    static constexpr float guideShelfWidthFraction = 0.256f;
    static constexpr float maximumGuideShelfWidth = 272.f;
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
    static bool isOverlayComponentVisible(
            juce::Rectangle<float> componentBounds,
            juce::Rectangle<float> overlayBounds);
    static juce::Rectangle<float> headerBounds(juce::Rectangle<float> shelf);
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
};

}
