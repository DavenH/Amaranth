#include "GuideCurveShelf.h"

#include "SignalProbeRail.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace CycleV2 {

namespace {

const Colour kShelfBackground { 0xf51a212a };
const Colour kShelfBorder { 0xff445261 };
const Colour kTileBackground { 0xff11171d };
const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kPadding = 12.f;
constexpr float kTileWidth = 220.f;
constexpr float kTileGap = 10.f;

Colour colourForGuide(const GuideCurveResource& guide) {
    static constexpr std::array<uint32, 6> colours {
            0xff79b8ff,
            0xffd2a8ff,
            0xff7ee787,
            0xffffc680,
            0xffff7b72,
            0xffa5d6ff
    };
    return Colour(colours[(size_t) std::abs(guide.colourIndex) % colours.size()]);
}

void paintRasterizedGuidePreview(
        Graphics& graphics,
        Effect2DWidget& preview,
        Rectangle<float> bounds) {
    const std::vector<CurvePreviewVertex> waveform = preview.rasterizedPreviewVertices();
    if (waveform.size() < 2) {
        return;
    }

    Graphics::ScopedSaveState clip(graphics);
    graphics.reduceClipRegion(bounds.toNearestInt());

    Path waveformPath;
    for (size_t index = 0; index < waveform.size(); ++index) {
        const CurvePreviewVertex& point = waveform[index];
        const Point<float> displayPoint(
                bounds.getX() + bounds.getWidth() * point.x,
                bounds.getCentreY() - bounds.getHeight() * 0.5f * point.y);
        if (index == 0) {
            waveformPath.startNewSubPath(displayPoint);
        } else {
            waveformPath.lineTo(displayPoint);
        }
    }

    Path fill(waveformPath);
    fill.lineTo(bounds.getRight(), bounds.getCentreY());
    fill.lineTo(bounds.getX(), bounds.getCentreY());
    fill.closeSubPath();
    graphics.setColour(Colour(0xffe2e8ef).withAlpha(0.20f));
    graphics.fillPath(fill);
    graphics.setColour(Colour(0xffe2e8ef).withAlpha(0.92f));
    graphics.strokePath(
            waveformPath,
            PathStrokeType(1.2f, PathStrokeType::curved, PathStrokeType::rounded));
}

}

Rectangle<float> GuideCurveShelf::guideWorkspace(
        Rectangle<float> workspace,
        float splitRatio,
        bool guidesMinimized,
        bool spiesMinimized) {
    if (guidesMinimized && !spiesMinimized) {
        return workspace.removeFromLeft(minimizedWidth);
    }
    if (spiesMinimized && !guidesMinimized) {
        workspace.removeFromRight(minimizedWidth);
        return workspace;
    }
    const float ratio = jlimit(0.2f, 0.8f, splitRatio);
    return workspace.removeFromLeft(workspace.getWidth() * ratio);
}

Rectangle<float> GuideCurveShelf::spyWorkspace(
        Rectangle<float> workspace,
        float splitRatio,
        bool guidesMinimized,
        bool spiesMinimized) {
    if (guidesMinimized && !spiesMinimized) {
        workspace.removeFromLeft(minimizedWidth);
        return workspace;
    }
    if (spiesMinimized && !guidesMinimized) {
        return workspace.removeFromRight(minimizedWidth);
    }
    const float ratio = jlimit(0.2f, 0.8f, splitRatio);
    workspace.removeFromLeft(workspace.getWidth() * ratio);
    return workspace;
}

Rectangle<float> GuideCurveShelf::boundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state) {
    Rectangle<float> guideWorkspaceBounds = guideWorkspace(
            workspace,
            splitRatio,
            state.minimized,
            dockState.minimized);
    Rectangle<float> dock = SignalProbeRail::boundsFor(guideWorkspaceBounds, dockState);
    if (!state.minimized) {
        return dock;
    }
    return dock.removeFromLeft(minimizedWidth);
}

Rectangle<float> GuideCurveShelf::addButtonBounds(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state) {
    Rectangle<float> header = boundsFor(workspace, dockState, splitRatio, state).reduced(kPadding, 8.f);
    header.removeFromTop(24.f);
    return header.removeFromRight(22.f);
}

Rectangle<float> GuideCurveShelf::minimizeButtonBounds(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state) {
    Rectangle<float> header = boundsFor(workspace, dockState, splitRatio, state).reduced(kPadding, 8.f);
    header.removeFromTop(24.f);
    return header.removeFromLeft(18.f);
}

String GuideCurveShelf::guideAt(
        Point<float> position,
        const NodeGraph& graph,
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state) {
    if (state.minimized) {
        return {};
    }
    const Rectangle<float> shelf = boundsFor(workspace, dockState, splitRatio, state);
    for (int index = 0; index < (int) graph.getGuideCurves().size(); ++index) {
        const Rectangle<float> tile(
                shelf.getX() + kPadding + index * (kTileWidth + kTileGap) - state.horizontalOffset,
                shelf.getY() + 42.f,
                kTileWidth,
                shelf.getHeight() - 54.f);
        if (tile.contains(position)) {
            return graph.getGuideCurves()[(size_t) index].id;
        }
    }
    return {};
}

float GuideCurveShelf::maximumHorizontalOffset(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state,
        int guideCount) {
    if (state.minimized || guideCount < 1) {
        return 0.f;
    }
    const Rectangle<float> shelf = boundsFor(workspace, dockState, splitRatio, state);
    const float contentWidth = kPadding * 2.f + guideCount * kTileWidth
            + jmax(0, guideCount - 1) * kTileGap;
    return jmax(0.f, contentWidth - shelf.getWidth());
}

Effect2DWidget& GuideCurveShelf::previewFor(const GuideCurveResource& guide) const {
    std::unique_ptr<Effect2DWidget>& preview = previews[guide.id];
    if (preview == nullptr) {
        preview = std::make_unique<Effect2DWidget>(true);
    }

    preview->syncFromGuideResource(guide);
    return *preview;
}

void GuideCurveShelf::paint(
        Graphics& graphics,
        const NodeGraph& graph,
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state) const {
    Rectangle<float> shelf = boundsFor(workspace, dockState, splitRatio, state);
    graphics.setColour(kShelfBackground);
    graphics.fillRect(shelf);
    graphics.setColour(kShelfBorder);
    graphics.drawRect(shelf, 1.f);

    if (!dockState.expanded) {
        return;
    }

    if (state.minimized) {
        graphics.setColour(kText);
        graphics.setFont(FontOptions(13.f, Font::bold));
        graphics.drawText(">", shelf, Justification::centred);
        return;
    }

    Rectangle<float> header = shelf.reduced(kPadding, 8.f).removeFromTop(24.f);
    const Rectangle<float> minimize = header.removeFromLeft(18.f);
    graphics.setColour(Colour(0xff26313d));
    graphics.fillRoundedRectangle(minimize, 4.f);
    graphics.setColour(kText);
    graphics.setFont(FontOptions(14.f));
    graphics.drawText("-", minimize, Justification::centred);
    graphics.setColour(kText);
    graphics.setFont(FontOptions(12.f, Font::bold));
    graphics.drawText(
            "Guides (" + String((int) graph.getGuideCurves().size()) + ")",
            header,
            Justification::centredLeft);
    const Rectangle<float> plus = header.removeFromRight(22.f);
    graphics.setColour(Colour(0xff26313d));
    graphics.fillRoundedRectangle(plus, 5.f);
    graphics.setColour(kText);
    graphics.setFont(FontOptions(16.f));
    graphics.drawText("+", plus, Justification::centred);

    if (graph.getGuideCurves().empty()) {
        Rectangle<float> vacancy(280.f, 92.f);
        vacancy = vacancy.withCentre(shelf.getCentre().withY(shelf.getCentreY() + 12.f));
        graphics.setColour(kTileBackground);
        graphics.fillRoundedRectangle(vacancy, 8.f);
        graphics.setColour(kShelfBorder.withAlpha(0.75f));
        graphics.drawRoundedRectangle(vacancy, 8.f, 1.f);
        graphics.setColour(kMutedText);
        graphics.setFont(FontOptions(13.f, Font::bold));
        graphics.drawText("No Guides", vacancy.removeFromTop(44.f), Justification::centred);
        return;
    }

    Graphics::ScopedSaveState clip(graphics);
    graphics.reduceClipRegion(shelf.toNearestInt());
    for (int index = 0; index < (int) graph.getGuideCurves().size(); ++index) {
        const GuideCurveResource& guide = graph.getGuideCurves()[(size_t) index];
        const Rectangle<float> tile(
                shelf.getX() + kPadding + index * (kTileWidth + kTileGap) - state.horizontalOffset,
                shelf.getY() + 42.f,
                kTileWidth,
                shelf.getHeight() - 54.f);
        graphics.setColour(kTileBackground);
        graphics.fillRoundedRectangle(tile, 7.f);
        graphics.setColour(guide.id == state.selectedGuideId ? Colour(0xff8ac4ff) : colourForGuide(guide));
        graphics.drawRoundedRectangle(tile, 7.f, guide.id == state.selectedGuideId ? 2.f : 1.f);
        Rectangle<float> thumbnail = tile.reduced(10.f);
        thumbnail.removeFromTop(34.f);
        thumbnail.removeFromBottom(12.f);
        graphics.setColour(Colour(0xff0d1117).withAlpha(0.72f));
        graphics.fillRoundedRectangle(thumbnail, 4.f);
        paintRasterizedGuidePreview(graphics, previewFor(guide), thumbnail.reduced(4.f, 5.f));
        String label = guide.shortLabel;
        if (!guide.name.isEmpty() && guide.name != "Guide Curve") {
            label += " · " + guide.name;
        }
        graphics.setColour(kText);
        graphics.setFont(FontOptions(12.f, Font::bold));
        graphics.drawText(label, tile.reduced(10.f).removeFromTop(24.f), Justification::centredLeft);
        const int usageCount = (int) std::count_if(
                graph.getGuideAssignments().begin(),
                graph.getGuideAssignments().end(),
                [&](const GuideCurveAssignment& assignment) {
                    return assignment.guideId == guide.id;
                });
        graphics.setColour(kMutedText);
        graphics.setFont(FontOptions(10.f));
        graphics.drawText(
                String(usageCount) + (usageCount == 1 ? " use" : " uses"),
                tile.reduced(10.f).removeFromBottom(18.f),
                Justification::centredRight);
    }
}

}
