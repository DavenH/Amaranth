#include "WorkspaceDock.h"

namespace CycleV2 {

namespace {

const juce::Colour kChromeBackground { 0xff26313d };
const juce::Colour kChromeBorder { 0xff445261 };
const juce::Colour kChromeText { 0xffe2e8ef };

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
                22.f)
                .withCentre(result.dock.getCentre());
        return result;
    }

    result.resizeHandle = result.dock.withHeight(7.f);
    result.collapseHandle = juce::Rectangle<float>(40.f, 18.f)
            .withCentre({ result.dock.getCentreX(), result.dock.getY() + 10.f });

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

void WorkspaceDock::paintChrome(
        juce::Graphics& graphics,
        const WorkspaceDockLayout& layout,
        const juce::String& leftSummary,
        const juce::String& rightSummary,
        bool expanded) {
    graphics.setColour(kChromeBorder);
    graphics.drawHorizontalLine(
            juce::roundToInt(layout.dock.getY()),
            layout.dock.getX(),
            layout.dock.getRight());
    graphics.setColour(kChromeBackground);
    graphics.fillRoundedRectangle(layout.collapseHandle, expanded ? 5.f : 7.f);
    graphics.setColour(kChromeBorder);
    graphics.drawRoundedRectangle(layout.collapseHandle, expanded ? 5.f : 7.f, 1.f);
    graphics.setColour(kChromeText);

    if (!expanded) {
        graphics.setFont(juce::FontOptions(11.f, juce::Font::bold));
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
    graphics.strokePath(chevron, juce::PathStrokeType(1.5f));
}

}
