#include "GuideCurveShelf.h"

#include "SignalProbeRail.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace CycleV2 {

namespace {

const Colour kShelfBackground { 0xf51a212a };
const Colour kShelfBorder { 0xff445261 };
const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };

bool hasDisplayName(const GuideCurveResource& guide) {
    return !guide.name.isEmpty() && guide.name != "Guide Curve";
}

Rectangle<float> previewBoundsFor(Rectangle<float> tile) {
    return tile.reduced(7.f);
}

WorkspaceDockState workspaceDockState(
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& guideState) {
    return {
            dockState.expanded,
            guideState.minimized,
            dockState.minimized,
            dockState.expandedHeight,
            splitRatio
    };
}

}

Colour GuideCurveShelf::colourForGuide(const GuideCurveResource& guide) {
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

Rectangle<float> GuideCurveShelf::guideWorkspace(
        Rectangle<float> workspace,
        float splitRatio,
        bool guidesMinimized,
        bool spiesMinimized) {
    const WorkspaceDockLayout layout = WorkspaceDock::layout(
            workspace,
            { true, guidesMinimized, spiesMinimized, 190.f, splitRatio });
    return layout.leftShelf.withY(workspace.getY()).withHeight(workspace.getHeight());
}

Rectangle<float> GuideCurveShelf::spyWorkspace(
        Rectangle<float> workspace,
        float splitRatio,
        bool guidesMinimized,
        bool spiesMinimized) {
    const WorkspaceDockLayout layout = WorkspaceDock::layout(
            workspace,
            { true, guidesMinimized, spiesMinimized, 190.f, splitRatio });
    return layout.rightShelf.withY(workspace.getY()).withHeight(workspace.getHeight());
}

Rectangle<float> GuideCurveShelf::boundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state) {
    return WorkspaceDock::layout(
            workspace,
            workspaceDockState(dockState, splitRatio, state)).leftShelf;
}

Rectangle<float> GuideCurveShelf::addButtonBounds(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state) {
    if (!dockState.expanded || state.minimized) {
        return {};
    }
    const Rectangle<float> header = WorkspaceDock::headerBounds(
            boundsFor(workspace, dockState, splitRatio, state));
    return { header.getX() + 120.f, header.getY(), WorkspaceDock::controlSize,
            WorkspaceDock::controlSize };
}

Rectangle<float> GuideCurveShelf::minimizeButtonBounds(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state) {
    if (!dockState.expanded || state.minimized) {
        return {};
    }
    Rectangle<float> header = WorkspaceDock::headerBounds(
            boundsFor(workspace, dockState, splitRatio, state));
    return header.removeFromLeft(WorkspaceDock::controlSize);
}

Rectangle<float> GuideCurveShelf::tileBoundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state,
        int tileIndex) {
    return WorkspaceDock::tileBounds(
            boundsFor(workspace, dockState, splitRatio, state),
            tileIndex,
            state.horizontalOffset);
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
    for (int index = 0; index < (int) graph.getGuideCurves().size(); ++index) {
        const Rectangle<float> tile = tileBoundsFor(
                workspace,
                dockState,
                splitRatio,
                state,
                index);
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
    const float contentWidth = WorkspaceDock::shelfPadding * 2.f
            + guideCount * WorkspaceDock::tileWidth
            + jmax(0, guideCount - 1) * WorkspaceDock::tileGap;
    return jmax(0.f, contentWidth - shelf.getWidth());
}

GuideCurveShelf::Preview& GuideCurveShelf::previewFor(const GuideCurveResource& guide) const {
    Preview& preview = previews[guide.id];
    if (preview.widget == nullptr) {
        preview.widget = std::make_unique<Effect2DWidget>(true);
    }

    if (preview.model != guide.model
            || preview.enabled != guide.enabled
            || preview.noise != guide.noise
            || preview.dcOffset != guide.dcOffset
            || preview.phase != guide.phase) {
        preview.widget->syncFromGuideResource(guide);
        preview.model = guide.model;
        preview.enabled = guide.enabled;
        preview.noise = guide.noise;
        preview.dcOffset = guide.dcOffset;
        preview.phase = guide.phase;
        preview.needsOpenGLRender = true;
    }
    return preview;
}

void GuideCurveShelf::paint(
        Graphics& graphics,
        const NodeGraph& graph,
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state,
        const WorkspaceDockFocus& focus) const {
    Rectangle<float> shelf = boundsFor(workspace, dockState, splitRatio, state);
    graphics.setColour(kShelfBackground);
    graphics.fillRect(shelf);
    graphics.setColour(kShelfBorder);
    graphics.drawRect(shelf, 1.f);

    if (!dockState.expanded) {
        return;
    }

    if (state.minimized) {
        Rectangle<float> drawerButton = shelf.removeFromTop(WorkspaceDock::drawerWidth).reduced(4.f);
        WorkspaceDock::paintIconButton(
                graphics,
                drawerButton,
                ">",
                focus.target == WorkspaceDockFocusTarget::GuideDrawer);
        Graphics::ScopedSaveState labelTransform(graphics);
        graphics.addTransform(AffineTransform::rotation(
                -MathConstants<float>::halfPi,
                shelf.getCentreX(),
                shelf.getCentreY()));
        graphics.drawText(
                "GUIDES (" + String((int) graph.getGuideCurves().size()) + ")",
                Rectangle<float>(shelf.getHeight() - 8.f, shelf.getWidth())
                        .withCentre(shelf.getCentre()),
                Justification::centred);
        return;
    }

    Rectangle<float> header = WorkspaceDock::headerBounds(shelf);
    const Rectangle<float> minimize = minimizeButtonBounds(
            workspace, dockState, splitRatio, state);
    WorkspaceDock::paintIconButton(
            graphics,
            minimize,
            "<",
            focus.target == WorkspaceDockFocusTarget::GuideMinimize);
    graphics.setColour(kText);
    graphics.setFont(FontOptions(12.f, Font::bold));
    graphics.drawText(
            "Guides (" + String((int) graph.getGuideCurves().size()) + ")",
            header.withTrimmedLeft(34.f).withWidth(82.f),
            Justification::centredLeft);
    const Rectangle<float> plus = addButtonBounds(workspace, dockState, splitRatio, state);
    WorkspaceDock::paintIconButton(
            graphics,
            plus,
            "+",
            focus.target == WorkspaceDockFocusTarget::GuideAdd);

    if (graph.getGuideCurves().empty()) {
        const Rectangle<float> vacancy = WorkspaceDock::vacancyBounds(shelf);
        graphics.setColour(Colour(0xff11171d).withAlpha(0.68f));
        graphics.fillRoundedRectangle(vacancy, 7.f);
        graphics.setColour(kShelfBorder.withAlpha(0.75f));
        graphics.drawRoundedRectangle(vacancy, 7.f, 1.f);
        graphics.setColour(kMutedText);
        graphics.setFont(FontOptions(12.f, Font::bold));
        graphics.drawText("No guides", vacancy.reduced(14.f), Justification::centredLeft);
        return;
    }

    Graphics::ScopedSaveState clip(graphics);
    graphics.reduceClipRegion(shelf.toNearestInt());
    for (int index = 0; index < (int) graph.getGuideCurves().size(); ++index) {
        const GuideCurveResource& guide = graph.getGuideCurves()[(size_t) index];
        const Rectangle<float> tile = tileBoundsFor(
                workspace,
                dockState,
                splitRatio,
                state,
                index);
        const bool selected = guide.id == state.selectedGuideId;
        const bool hovered = guide.id == state.hoveredGuideId;
        const bool focused = focus.target == WorkspaceDockFocusTarget::GuideTile
                && focus.itemId == guide.id;
        WorkspaceDock::paintTileChrome(
                graphics,
                tile,
                kShelfBorder,
                selected,
                hovered,
                focused);
        const Rectangle<float> thumbnail = previewBoundsFor(tile);
        graphics.setColour(Colour(0xff0d1117).withAlpha(0.72f));
        graphics.fillRoundedRectangle(thumbnail, 4.f);
        Preview& preview = previewFor(guide);
        preview.widget->paintPreviewSnapshot(graphics, thumbnail);
        if (hasDisplayName(guide)) {
            graphics.setColour(kText);
            graphics.setFont(FontOptions(12.f, Font::bold));
            graphics.drawText(
                    guide.name,
                    thumbnail.reduced(8.f).removeFromTop(22.f),
                    Justification::centredLeft);
        }
    }
    WorkspaceDock::paintOverflowFeedback(
            graphics,
            shelf,
            state.horizontalOffset,
            maximumHorizontalOffset(
                    workspace,
                    dockState,
                    splitRatio,
                    state,
                    (int) graph.getGuideCurves().size()));
}

bool GuideCurveShelf::needsOpenGLPreviewRender() const {
    for (const auto& entry : previews) {
        if (entry.second.needsOpenGLRender) {
            return true;
        }
    }
    return false;
}

bool GuideCurveShelf::renderOpenGL(
        const NodeGraph& graph,
        Rectangle<float> workspace,
        Rectangle<float> captureWorkspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state,
        float scaleFactor) {
    if (!dockState.expanded || state.minimized) {
        return false;
    }

    bool rendered {};
    const Rectangle<float> shelf = boundsFor(workspace, dockState, splitRatio, state);
    for (int index = 0; index < (int) graph.getGuideCurves().size(); ++index) {
        const GuideCurveResource& guide = graph.getGuideCurves()[(size_t) index];
        Preview& preview = previewFor(guide);
        if (!preview.needsOpenGLRender) {
            continue;
        }

        const Rectangle<float> tile = tileBoundsFor(
                workspace,
                dockState,
                splitRatio,
                state,
                index);
        const Rectangle<float> thumbnail = previewBoundsFor(tile);
        Rectangle<float> captureBounds(
                captureWorkspace.getX() + 4.f,
                captureWorkspace.getY() + 4.f,
                thumbnail.getWidth(),
                thumbnail.getHeight());
        preview.widget->renderGuidePreviewSnapshotOpenGL(captureBounds, scaleFactor);
        preview.needsOpenGLRender = false;
        rendered = true;
    }
    return rendered;
}

}
