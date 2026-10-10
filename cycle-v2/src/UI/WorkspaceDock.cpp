#include <algorithm>

#include "UI/WorkspaceDock.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/CanvasUtilityDock.h"

namespace CycleV2 {

namespace {

void paintIconGlyph(
        juce::Graphics& graphics,
        juce::Rectangle<float> bounds,
        WorkspaceDockIcon icon) {
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    juce::Path path;
    if (icon == WorkspaceDockIcon::Add) {
        path.startNewSubPath(centreX - 5.f, centreY);
        path.lineTo(centreX + 5.f, centreY);
        path.startNewSubPath(centreX, centreY - 5.f);
        path.lineTo(centreX, centreY + 5.f);
    } else {
        const float direction = icon == WorkspaceDockIcon::ChevronRight ? 1.f : -1.f;
        path.startNewSubPath(centreX - 3.f * direction, centreY - 5.f);
        path.lineTo(centreX + 3.f * direction, centreY);
        path.lineTo(centreX - 3.f * direction, centreY + 5.f);
    }
    graphics.strokePath(path, juce::PathStrokeType(
            1.8f,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
}

}

WorkspaceDockLayout WorkspaceDock::layout(
        juce::Rectangle<float> workspace,
        const WorkspaceDockState& state) {
    WorkspaceDockLayout result;
    result.workspace = workspace;
    result.content = workspace;
    if (workspace.isEmpty()) {
        return result;
    }

    const float guideWidth = juce::jmin(maximumGuideShelfWidth,
            juce::jmax(drawerWidth, workspace.getWidth() * guideShelfWidthFraction));
    const float activeGuideWidth = state.leftMinimized ? drawerWidth : guideWidth;
    const float guideLeft = workspace.getX() + CanvasUtilityDock::margin;
    const float guideTop = workspace.getY() + CanvasUtilityDock::margin;
    const float guideHeight = juce::jmax(0.f,
            workspace.getBottom() - CanvasUtilityDock::margin - guideTop);
    result.leftShelf = { guideLeft,
            guideTop, activeGuideWidth, guideHeight };
    return result;
}

juce::Rectangle<float> WorkspaceDock::editorAvailableBounds(const WorkspaceDockLayout& layout) {
    if (layout.leftShelf.isEmpty()) {
        return layout.content;
    }
    return layout.content.withLeft(juce::jmin(
            layout.content.getRight(), layout.leftShelf.getRight() + CanvasUtilityDock::gap));
}

bool WorkspaceDock::isOverlayComponentVisible(
        juce::Rectangle<float> componentBounds,
        juce::Rectangle<float> overlayBounds) {
    return overlayBounds.isEmpty() || !overlayBounds.intersects(componentBounds);
}

juce::Rectangle<float> WorkspaceDock::headerBounds(juce::Rectangle<float> shelf) {
    return {
            shelf.getX() + shelfPadding,
            shelf.getY() + 5.f,
            juce::jmax(0.f, shelf.getWidth() - shelfPadding * 2.f),
            controlSize
    };
}

juce::Rectangle<float> WorkspaceDock::guideTileBounds(
        juce::Rectangle<float> shelf,
        int tileIndex,
        float verticalOffset) {
    return {
            shelf.getX() + shelfPadding,
            shelf.getY() + headerHeight
                    + (float) tileIndex * (guideTileHeight + tileGap) - verticalOffset,
            juce::jmax(0.f, shelf.getWidth() - shelfPadding * 2.f),
            guideTileHeight
    };
}

float WorkspaceDock::offsetToRevealGuideTile(
        float currentOffset,
        float maximumOffset,
        float shelfHeight,
        int tileIndex) {
    const float tileTop = headerHeight + (float) tileIndex * (guideTileHeight + tileGap);
    const float visibleBottom = currentOffset + shelfHeight - tileBottomPadding;
    if (tileTop < currentOffset + headerHeight) {
        currentOffset = tileTop - headerHeight;
    } else if (tileTop + guideTileHeight > visibleBottom) {
        currentOffset = tileTop + guideTileHeight - shelfHeight + tileBottomPadding;
    }
    return juce::jlimit(0.f, maximumOffset, currentOffset);
}

juce::Rectangle<float> WorkspaceDock::vacancyBounds(juce::Rectangle<float> shelf) {
    const float width = juce::jmin(tileWidth, juce::jmax(40.f, shelf.getWidth() - shelfPadding * 2.f));
    return {
            shelf.getX() + shelfPadding,
            shelf.getY() + headerHeight,
            width,
            juce::jmin(58.f, juce::jmax(36.f, shelf.getHeight() - headerHeight - tileBottomPadding))
    };
}

WorkspaceDockFocus WorkspaceDock::advanceFocus(
        const std::vector<WorkspaceDockFocus>& order,
        const WorkspaceDockFocus& current,
        int direction) {
    if (order.empty()) {
        return {};
    }

    const auto found = std::find(order.begin(), order.end(), current);
    if (found == order.end()) {
        return direction < 0 ? order.back() : order.front();
    }

    const int currentIndex = (int) std::distance(order.begin(), found);
    const int count = (int) order.size();
    const int nextIndex = (currentIndex + (direction < 0 ? count - 1 : 1)) % count;
    return order[(size_t) nextIndex];
}

void WorkspaceDock::paintIconButton(
        juce::Graphics& graphics,
        juce::Rectangle<float> bounds,
        WorkspaceDockIcon icon,
        bool focused) {
    const auto colours = CanvasChromePalette::control(focused
            ? CanvasChromeControlState::Focused
            : CanvasChromeControlState::Resting);
    graphics.setColour(colours.surface);
    graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::controlCornerRadius);
    graphics.setColour(colours.border);
    graphics.drawRoundedRectangle(
            bounds,
            CanvasChromeMetrics::controlCornerRadius,
            focused
                    ? CanvasChromeMetrics::focusRingWidth
                    : CanvasChromeMetrics::restingBorderWidth);
    graphics.setColour(colours.text);
    paintIconGlyph(graphics, bounds, icon);
}

void WorkspaceDock::paintTileChrome(
        juce::Graphics& graphics,
        juce::Rectangle<float> tile,
        bool selected,
        bool hovered,
        bool focused) {
    graphics.setColour(CanvasChromePalette::insetBackground);
    graphics.fillRoundedRectangle(tile, CanvasChromeMetrics::tileCornerRadius);

    const bool active = selected || hovered || focused;
    const juce::Colour border = active
            ? CanvasChromePalette::selectionOutline
            : CanvasChromePalette::border.withAlpha(0.56f);
    graphics.setColour(border);
    graphics.drawRoundedRectangle(
            tile,
            CanvasChromeMetrics::tileCornerRadius,
            active
                    ? CanvasChromeMetrics::activeBorderWidth
                    : CanvasChromeMetrics::restingBorderWidth);
    if (focused) {
        graphics.setColour(CanvasChromePalette::focus.withAlpha(0.9f));
        graphics.drawRoundedRectangle(
                tile.reduced(3.f),
                CanvasChromeMetrics::controlCornerRadius,
                CanvasChromeMetrics::focusRingWidth);
    }

    if (selected) {
        graphics.setColour(CanvasChromePalette::selectionOutline.withAlpha(0.16f));
        graphics.drawRoundedRectangle(
                tile.expanded(2.f),
                CanvasChromeMetrics::tileCornerRadius + 2.f,
                2.f);
    }
}

void WorkspaceDock::paintVerticalOverflowFeedback(
        juce::Graphics& graphics,
        juce::Rectangle<float> shelf,
        float verticalOffset,
        float maximumOffset) {
    if (maximumOffset <= 0.f) {
        return;
    }
    const auto track = shelf.withTrimmedRight(5.f).removeFromRight(3.f)
            .withTrimmedTop(headerHeight)
            .withTrimmedBottom(tileBottomPadding);
    const float visibleHeight = shelf.getHeight() - headerHeight - tileBottomPadding;
    const float thumbHeight = juce::jmax(22.f,
            track.getHeight() * visibleHeight / (visibleHeight + maximumOffset));
    const float travel = juce::jmax(0.f, track.getHeight() - thumbHeight);
    graphics.setColour(CanvasChromePalette::border.withAlpha(0.5f));
    graphics.fillRoundedRectangle(track, 1.5f);
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.7f));
    graphics.fillRoundedRectangle({ track.getX(),
            track.getY() + travel * verticalOffset / maximumOffset,
            track.getWidth(), thumbHeight }, 1.5f);
}

}
