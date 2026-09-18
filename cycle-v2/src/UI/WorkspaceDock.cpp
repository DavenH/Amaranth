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

    const CanvasUtilityDockLayout utilities = CanvasUtilityDock::layout(workspace);
    const float guideWidth = juce::jmin(tileWidth + shelfPadding * 2.f,
            juce::jmax(drawerWidth, workspace.getWidth() * 0.32f));
    const float activeGuideWidth = state.leftMinimized ? drawerWidth : guideWidth;
    const float guideRight = workspace.getRight() - CanvasUtilityDock::margin;
    const float guideTop = utilities.legend.getBottom() + CanvasUtilityDock::gap;
    const float spyRight = juce::jmax(workspace.getX(),
            guideRight - activeGuideWidth - CanvasUtilityDock::gap);
    result.dock = spyRowBounds(
            workspace.withRight(spyRight), state.expanded, state.expandedHeight);

    if (!state.expanded) {
        result.collapseHandle = juce::Rectangle<float>(
                juce::jmin(220.f, juce::jmax(40.f, result.dock.getWidth() - 24.f)),
                28.f)
                .withCentre({ result.dock.getCentreX(), workspace.getBottom() - 17.f });
        return result;
    }

    result.resizeHandle = { result.dock.getX() + shelfPadding,
            result.dock.getY(), 100.f, 5.f };
    result.collapseHandle = state.rightMinimized
            ? juce::Rectangle<float>(result.dock.getX() + drawerWidth + tileGap,
                    result.dock.getY() + 5.f, controlSize, controlSize)
            : spyControls(result.dock).collapse;
    const float guideHeight = juce::jmax(0.f,
            workspace.getBottom() - CanvasUtilityDock::margin - guideTop);
    result.leftShelf = { guideRight - activeGuideWidth,
            guideTop, activeGuideWidth, guideHeight };
    result.rightShelf = result.dock;
    if (state.rightMinimized) {
        result.rightShelf = result.dock.withWidth(drawerWidth);
    }
    return result;
}

juce::Rectangle<float> WorkspaceDock::editorAvailableBounds(const WorkspaceDockLayout& layout) {
    if (layout.leftShelf.isEmpty()) {
        return layout.content;
    }
    return layout.content.withRight(juce::jmax(
            layout.content.getX(), layout.leftShelf.getX() - CanvasUtilityDock::gap));
}

WorkspaceDockSpyControls WorkspaceDock::spyControls(juce::Rectangle<float> rail) {
    WorkspaceDockSpyControls controls;
    const float usableWidth = juce::jmax(0.f, rail.getWidth() - shelfPadding * 2.f);
    const float gap = 6.f;
    const float width = juce::jmin(258.f, usableWidth);
    const float labelWidth = juce::jmin(84.f,
            juce::jmax(52.f, width - 104.f - controlSize * 2.f - gap * 3.f));
    const float refreshWidth = juce::jmax(0.f,
            width - labelWidth - controlSize * 2.f - gap * 3.f);
    const float x = rail.getX() + shelfPadding;
    const float y = rail.getY() + 5.f;
    controls.label = { x, y, labelWidth, controlSize };
    controls.refresh = { controls.label.getRight() + gap, y, refreshWidth, controlSize };
    controls.minimize = { controls.refresh.getRight() + gap, y, controlSize, controlSize };
    controls.collapse = { controls.minimize.getRight() + gap, y, controlSize, controlSize };
    return controls;
}

juce::Rectangle<float> WorkspaceDock::spyRowBounds(
        juce::Rectangle<float> workspace,
        bool expanded,
        float expandedHeight) {
    const float maximumHeight = juce::jmax(minimumExpandedHeight, workspace.getHeight() * 0.4f);
    const float height = expanded
            ? juce::jlimit(minimumExpandedHeight, maximumHeight, expandedHeight)
            : collapsedHeight;
    return workspace.removeFromBottom(height);
}

juce::Rectangle<float> WorkspaceDock::headerBounds(juce::Rectangle<float> shelf) {
    return {
            shelf.getX() + shelfPadding,
            shelf.getY() + 5.f,
            juce::jmax(0.f, shelf.getWidth() - shelfPadding * 2.f),
            controlSize
    };
}

juce::Rectangle<float> WorkspaceDock::tileBounds(
        juce::Rectangle<float> shelf,
        int tileIndex,
        float horizontalOffset) {
    return {
            shelf.getX() + shelfPadding
                    + (float) tileIndex * (tileWidth + tileGap)
                    - horizontalOffset,
            shelf.getY() + headerHeight,
            tileWidth,
            juce::jmax(0.f, shelf.getHeight() - headerHeight - tileBottomPadding)
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

float WorkspaceDock::offsetToRevealTile(
        float currentOffset,
        float maximumOffset,
        float shelfWidth,
        int tileIndex) {
    const float tileLeft = shelfPadding + (float) tileIndex * (tileWidth + tileGap);
    const float tileRight = tileLeft + tileWidth;
    const float visibleLeft = currentOffset + shelfPadding;
    const float visibleRight = currentOffset + shelfWidth - shelfPadding;
    float result = currentOffset;
    if (tileLeft < visibleLeft) {
        result = tileLeft - shelfPadding;
    } else if (tileRight > visibleRight) {
        result = tileRight - shelfWidth + shelfPadding;
    }
    return juce::jlimit(0.f, maximumOffset, result);
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
        juce::Colour token,
        bool selected,
        bool hovered,
        bool focused) {
    graphics.setColour(CanvasChromePalette::insetBackground);
    graphics.fillRoundedRectangle(tile, CanvasChromeMetrics::tileCornerRadius);

    const bool active = selected || hovered || focused;
    const juce::Colour border = active ? token.brighter(0.15f) : token;
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
}

void WorkspaceDock::paintOverflowFeedback(
        juce::Graphics& graphics,
        juce::Rectangle<float> shelf,
        float horizontalOffset,
        float maximumHorizontalOffset) {
    if (maximumHorizontalOffset <= 0.f) {
        return;
    }

    const juce::Rectangle<float> track = shelf.reduced(shelfPadding, 0.f)
            .removeFromBottom(3.f);
    const float visibleWidth = shelf.getWidth();
    const float contentWidth = visibleWidth + maximumHorizontalOffset;
    const float thumbWidth = juce::jmax(28.f, track.getWidth() * visibleWidth / contentWidth);
    const float travel = juce::jmax(0.f, track.getWidth() - thumbWidth);
    const float progress = horizontalOffset / maximumHorizontalOffset;
    const float trackCornerRadius = track.getHeight() * 0.5f;

    graphics.setColour(CanvasChromePalette::border.withAlpha(0.35f));
    graphics.fillRoundedRectangle(track, trackCornerRadius);
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.62f));
    graphics.fillRoundedRectangle(
            { track.getX() + progress * travel, track.getY(), thumbWidth, track.getHeight() },
            trackCornerRadius);

    graphics.setColour(CanvasChromePalette::dockSurface.withAlpha(0.8f));
    if (horizontalOffset > 0.f) {
        graphics.fillRect(shelf.getX(), shelf.getY() + headerHeight, 6.f,
                shelf.getHeight() - headerHeight);
    }
    if (horizontalOffset < maximumHorizontalOffset) {
        graphics.fillRect(shelf.getRight() - 6.f, shelf.getY() + headerHeight, 6.f,
                shelf.getHeight() - headerHeight);
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
    const auto track = shelf.removeFromRight(3.f).withTrimmedTop(headerHeight)
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

void WorkspaceDock::paintChrome(
        juce::Graphics& graphics,
        const WorkspaceDockLayout& layout,
        const juce::String& leftSummary,
        const juce::String& rightSummary,
        bool expanded,
        bool focused) {
    graphics.setColour(CanvasChromePalette::dockSurface);
    const float handleCornerRadius = expanded
            ? CanvasChromeMetrics::controlCornerRadius
            : CanvasChromeMetrics::tileCornerRadius;
    graphics.fillRoundedRectangle(layout.collapseHandle, handleCornerRadius);
    graphics.setColour(focused
            ? CanvasChromePalette::focus
            : CanvasChromePalette::border);
    graphics.drawRoundedRectangle(
            layout.collapseHandle,
            handleCornerRadius,
            focused
                    ? CanvasChromeMetrics::focusRingWidth
                    : CanvasChromeMetrics::restingBorderWidth);
    graphics.setColour(CanvasChromePalette::text);

    if (!expanded) {
        graphics.setFont(juce::FontOptions(CanvasChromeMetrics::captionFontSize));
        graphics.drawText(
                leftSummary + "  ·  " + rightSummary,
                layout.collapseHandle.withTrimmedLeft(28.f).withTrimmedRight(8.f),
                juce::Justification::centred);
    }

    const float centreX = expanded
            ? layout.collapseHandle.getCentreX()
            : layout.collapseHandle.getX() + 14.f;
    const float centreY = layout.collapseHandle.getCentreY();
    const float direction = expanded ? 1.f : -1.f;
    juce::Path chevron;
    chevron.startNewSubPath(centreX - 5.f, centreY - 2.f * direction);
    chevron.lineTo(centreX, centreY + 3.f * direction);
    chevron.lineTo(centreX + 5.f, centreY - 2.f * direction);
    graphics.strokePath(chevron, juce::PathStrokeType(
            1.5f,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
}

}
