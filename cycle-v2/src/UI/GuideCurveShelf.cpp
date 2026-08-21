#include "GuideCurveShelf.h"

#include "SignalProbeRail.h"

#include "../Nodes/Effect2D/CurveNodeModels.h"

#include <Curve/Mesh/Vertex.h>

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
constexpr float kTileWidth = 132.f;
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

void paintCurveThumbnail(
        Graphics& graphics,
        const GuideCurveResource& guide,
        Rectangle<float> bounds) {
    const auto model = std::dynamic_pointer_cast<const CurveNodeModelState>(guide.model);
    if (model == nullptr || model->flatCurve() == nullptr) {
        return;
    }

    std::vector<const Vertex*> vertices;
    for (const Vertex* vertex : model->flatCurve()->getMesh().getVerts()) {
        if (vertex != nullptr) {
            vertices.push_back(vertex);
        }
    }
    std::sort(vertices.begin(), vertices.end(), [](const Vertex* left, const Vertex* right) {
        return left->values[Vertex::Phase] < right->values[Vertex::Phase];
    });
    if (vertices.size() < 2) {
        return;
    }

    Path curve;
    for (size_t index = 0; index < vertices.size(); ++index) {
        const Vertex& vertex = *vertices[index];
        const Point<float> point(
                bounds.getX() + bounds.getWidth() * vertex.values[Vertex::Phase],
                bounds.getBottom() - bounds.getHeight() * vertex.values[Vertex::Amp]);
        if (index == 0) {
            curve.startNewSubPath(point);
        } else {
            curve.lineTo(point);
        }
    }
    graphics.setColour(colourForGuide(guide).withAlpha(0.92f));
    graphics.strokePath(curve, PathStrokeType(1.5f, PathStrokeType::curved));
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
    graphics.drawText("Guides", header, Justification::centredLeft);
    const Rectangle<float> plus = header.removeFromRight(22.f);
    graphics.setColour(Colour(0xff26313d));
    graphics.fillRoundedRectangle(plus, 5.f);
    graphics.setColour(kText);
    graphics.setFont(FontOptions(16.f));
    graphics.drawText("+", plus, Justification::centred);

    if (graph.getGuideCurves().empty()) {
        Rectangle<float> vacancy = shelf.reduced(24.f, 42.f);
        graphics.setColour(kTileBackground);
        graphics.fillRoundedRectangle(vacancy, 8.f);
        graphics.setColour(kShelfBorder.withAlpha(0.75f));
        graphics.drawRoundedRectangle(vacancy, 8.f, 1.f);
        graphics.setColour(kMutedText);
        graphics.setFont(FontOptions(12.f));
        graphics.drawText("[ ]", vacancy.removeFromTop(28.f), Justification::centred);
        graphics.drawText("No Guides - + Add Guide", vacancy, Justification::centred);
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
        const Rectangle<float> thumbnail = tile.reduced(10.f).withTrimmedTop(48.f).removeFromBottom(12.f);
        graphics.setColour(Colour(0xff0d1117).withAlpha(0.72f));
        graphics.fillRoundedRectangle(thumbnail, 4.f);
        paintCurveThumbnail(graphics, guide, thumbnail.reduced(4.f, 5.f));
        graphics.setColour(kText);
        graphics.setFont(FontOptions(12.f, Font::bold));
        graphics.drawText(guide.shortLabel, tile.reduced(10.f).removeFromTop(24.f), Justification::centredLeft);
        graphics.setColour(kMutedText);
        graphics.setFont(FontOptions(11.f));
        graphics.drawText(
                guide.name.isEmpty() ? "Guide Curve" : guide.name,
                tile.reduced(10.f).withTrimmedTop(28.f),
                Justification::centredLeft);
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
