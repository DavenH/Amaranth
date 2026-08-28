#include <algorithm>

#include "UI/WorkspaceDock.h"

#include "UI/CanvasChromePalette.h"

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

float WorkspaceDock::clampedSplitRatio(
        juce::Rectangle<float> workspace,
        float splitRatio) {
    if (workspace.getWidth() <= 0.f) {
        return 0.5f;
    }

    const float minimumWidth = juce::jmin(minimumShelfWidth, workspace.getWidth() * 0.5f);
    const float minimumRatio = minimumWidth / workspace.getWidth();
    return juce::jlimit(minimumRatio, 1.f - minimumRatio, splitRatio);
}

WorkspaceDockLayout WorkspaceDock::layout(
        juce::Rectangle<float> workspace,
        const WorkspaceDockState& state) {
    WorkspaceDockLayout result;
    result.workspace = workspace;

    const float maximumHeight = juce::jmax(
            minimumExpandedHeight,
            workspace.getHeight() * 0.4f);
    const float height = state.expanded
            ? juce::jlimit(minimumExpandedHeight, maximumHeight, state.expandedHeight)
            : collapsedHeight;
    result.dock = workspace.removeFromBottom(height);
    result.content = workspace;

    if (!state.expanded) {
        result.rightShelf = result.dock;
        result.collapseHandle = juce::Rectangle<float>(
                juce::jmin(280.f, juce::jmax(40.f, result.dock.getWidth() - 24.f)),
                28.f)
                .withCentre(result.dock.getCentre());
        return result;
    }

    result.resizeHandle = result.dock.withHeight(7.f);
    result.collapseHandle = juce::Rectangle<float>(40.f, 24.f)
            .withCentre({ result.dock.getCentreX(), result.dock.getY() + 12.f });

    juce::Rectangle<float> shelves = result.dock;
    if (state.leftMinimized && !state.rightMinimized) {
        result.leftShelf = shelves.removeFromLeft(drawerWidth);
        result.rightShelf = shelves;
        return result;
    }
    if (state.rightMinimized && !state.leftMinimized) {
        result.rightShelf = shelves.removeFromRight(drawerWidth);
        result.leftShelf = shelves;
        return result;
    }

    const float ratio = clampedSplitRatio(shelves, state.splitRatio);
    result.leftShelf = shelves.removeFromLeft(shelves.getWidth() * ratio);
    result.rightShelf = shelves;
    result.divider = {
            result.leftShelf.getRight() - 4.f,
            result.dock.getY(),
            8.f,
            result.dock.getHeight()
    };
    return result;
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
    graphics.fillRoundedRectangle(bounds, 5.f);
    graphics.setColour(colours.border);
    graphics.drawRoundedRectangle(bounds, 5.f, focused ? 2.f : 1.f);
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
    graphics.fillRoundedRectangle(tile, 7.f);

    const bool active = selected || hovered || focused;
    const juce::Colour border = active ? token.brighter(0.15f) : token;
    graphics.setColour(border);
    graphics.drawRoundedRectangle(tile, 7.f, active ? 2.f : 1.f);
    if (focused) {
        graphics.setColour(CanvasChromePalette::focus.withAlpha(0.9f));
        graphics.drawRoundedRectangle(tile.reduced(3.f), 5.f, 1.f);
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

    graphics.setColour(CanvasChromePalette::border.withAlpha(0.35f));
    graphics.fillRoundedRectangle(track, 1.5f);
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.62f));
    graphics.fillRoundedRectangle(
            { track.getX() + progress * travel, track.getY(), thumbWidth, track.getHeight() },
            1.5f);

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

void WorkspaceDock::paintChrome(
        juce::Graphics& graphics,
        const WorkspaceDockLayout& layout,
        const juce::String& leftSummary,
        const juce::String& rightSummary,
        bool expanded,
        bool focused) {
    graphics.setColour(CanvasChromePalette::border);
    graphics.drawHorizontalLine(
            juce::roundToInt(layout.dock.getY()),
            layout.dock.getX(),
            layout.dock.getRight());
    graphics.setColour(CanvasChromePalette::dockSurface);
    graphics.fillRoundedRectangle(layout.collapseHandle, expanded ? 5.f : 7.f);
    graphics.setColour(focused
            ? CanvasChromePalette::focus
            : CanvasChromePalette::border);
    graphics.drawRoundedRectangle(
            layout.collapseHandle,
            expanded ? 5.f : 7.f,
            focused ? 2.f : 1.f);
    graphics.setColour(CanvasChromePalette::text);

    if (!expanded) {
        graphics.setFont(juce::FontOptions(11.f));
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
