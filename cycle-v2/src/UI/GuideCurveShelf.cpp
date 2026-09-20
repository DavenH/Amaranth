#include <algorithm>
#include <array>
#include <cstdlib>

#include "UI/GuideCurveShelf.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/SignalProbeRail.h"

namespace CycleV2 {

namespace {

bool hasDisplayName(const GuideCurveResource& guide) {
    return !guide.name.isEmpty() && guide.name != "Guide Curve";
}

Rectangle<float> previewBoundsFor(Rectangle<float> tile) {
    return tile.reduced(7.f);
}

WorkspaceDockState workspaceDockState(
        const SignalProbeRailState& dockState,
        const GuideCurveShelfState& guideState) {
    return {
            dockState.expanded,
            guideState.minimized,
            dockState.minimized,
            dockState.expandedHeight
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
        bool guidesMinimized,
        bool spiesMinimized) {
    const WorkspaceDockLayout layout = WorkspaceDock::layout(
            workspace,
            { true, guidesMinimized, spiesMinimized, 190.f });
    return layout.leftShelf.withY(workspace.getY()).withHeight(workspace.getHeight());
}

Rectangle<float> GuideCurveShelf::spyWorkspace(
        Rectangle<float> workspace,
        bool guidesMinimized,
        bool spiesMinimized) {
    const WorkspaceDockLayout layout = WorkspaceDock::layout(
            workspace,
            { true, guidesMinimized, spiesMinimized, 190.f });
    return layout.rightShelf.withY(workspace.getY()).withHeight(workspace.getHeight());
}

Rectangle<float> GuideCurveShelf::boundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        const GuideCurveShelfState& state) {
    return WorkspaceDock::layout(
            workspace,
            workspaceDockState(dockState, state)).leftShelf;
}

Rectangle<float> GuideCurveShelf::addButtonBounds(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        const GuideCurveShelfState& state) {
    if (!dockState.expanded || state.minimized) {
        return {};
    }
    Rectangle<float> header = WorkspaceDock::headerBounds(
            boundsFor(workspace, dockState, state));
    return header.removeFromRight(WorkspaceDock::addButtonWidth);
}

Rectangle<float> GuideCurveShelf::minimizeButtonBounds(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        const GuideCurveShelfState& state) {
    if (!dockState.expanded || state.minimized) {
        return {};
    }
    Rectangle<float> header = WorkspaceDock::headerBounds(
            boundsFor(workspace, dockState, state));
    return header.removeFromLeft(WorkspaceDock::controlSize);
}

Rectangle<float> GuideCurveShelf::tileBoundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        const GuideCurveShelfState& state,
        int tileIndex) {
    return WorkspaceDock::guideTileBounds(
            boundsFor(workspace, dockState, state),
            tileIndex,
            state.verticalOffset);
}

String GuideCurveShelf::guideAt(
        Point<float> position,
        const NodeGraph& graph,
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        const GuideCurveShelfState& state) {
    if (state.minimized) {
        return {};
    }
    const Rectangle<float> visibleTiles = boundsFor(workspace, dockState, state)
            .withTrimmedTop(WorkspaceDock::headerHeight);
    if (visibleTiles.getHeight()
            < WorkspaceDock::guideTileHeight + WorkspaceDock::tileBottomPadding) {
        return {};
    }
    if (!visibleTiles.contains(position)) {
        return {};
    }
    for (int index = 0; index < (int) graph.getGuideCurves().size(); ++index) {
        const Rectangle<float> tile = tileBoundsFor(
                workspace,
                dockState,
                state,
                index);
        if (tile.contains(position)) {
            return graph.getGuideCurves()[(size_t) index].id;
        }
    }
    return {};
}

float GuideCurveShelf::maximumVerticalOffset(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        const GuideCurveShelfState& state,
        int guideCount) {
    if (state.minimized || guideCount < 1) {
        return 0.f;
    }
    const Rectangle<float> shelf = boundsFor(workspace, dockState, state);
    if (shelf.getHeight() < WorkspaceDock::headerHeight
            + WorkspaceDock::guideTileHeight + WorkspaceDock::tileBottomPadding) {
        return 0.f;
    }
    const float contentHeight = WorkspaceDock::headerHeight
            + guideCount * WorkspaceDock::guideTileHeight
            + jmax(0, guideCount - 1) * WorkspaceDock::tileGap
            + WorkspaceDock::tileBottomPadding;
    return jmax(0.f, contentHeight - shelf.getHeight());
}

GuideCurveShelf::Preview& GuideCurveShelf::previewFor(
        const GuideCurveResource& guide,
        const NodeGraph& graph) const {
    Preview& preview = previews[guide.id];
    if (preview.widget == nullptr) {
        preview.widget = std::make_unique<CurveEditorWidget>(true);
    }

    if (preview.model != guide.model
            || preview.enabled != guide.enabled
            || preview.noise != guide.noise
            || preview.dcOffset != guide.dcOffset
            || preview.phase != guide.phase
            || preview.heatmapAssetId != guide.heatmapAssetId) {
        preview.widget->syncFromGuideResource(
                guide,
                graph.guideHeatmapAsset(guide.heatmapAssetId));
        preview.model = guide.model;
        preview.enabled = guide.enabled;
        preview.noise = guide.noise;
        preview.dcOffset = guide.dcOffset;
        preview.phase = guide.phase;
        preview.heatmapAssetId = guide.heatmapAssetId;
        preview.needsOpenGLRender = true;
    }
    return preview;
}

void GuideCurveShelf::paint(
        Graphics& graphics,
        const NodeGraph& graph,
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        const GuideCurveShelfState& state,
        const WorkspaceDockFocus& focus) const {
    Rectangle<float> shelf = boundsFor(workspace, dockState, state);
    if (!dockState.expanded) {
        return;
    }

    if (state.minimized) {
        graphics.setColour(CanvasChromePalette::dockSurface.withAlpha(0.92f));
        graphics.fillRoundedRectangle(shelf, CanvasChromeMetrics::panelCornerRadius);
        Rectangle<float> drawerButton = shelf.removeFromTop(WorkspaceDock::drawerWidth).reduced(4.f);
        WorkspaceDock::paintIconButton(
                graphics,
                drawerButton,
                WorkspaceDockIcon::ChevronLeft,
                focus.target == WorkspaceDockFocusTarget::GuideDrawer);
        Graphics::ScopedSaveState labelTransform(graphics);
        graphics.addTransform(AffineTransform::rotation(
                -MathConstants<float>::halfPi,
                shelf.getCentreX(),
                shelf.getCentreY()));
        graphics.drawText(
                "CURVE GUIDES",
                Rectangle<float>(shelf.getHeight() - 8.f, shelf.getWidth())
                        .withCentre(shelf.getCentre()),
                Justification::centred);
        return;
    }

    Rectangle<float> header = WorkspaceDock::headerBounds(shelf);
    graphics.setColour(CanvasChromePalette::dockSurface.withAlpha(0.94f));
    graphics.fillRoundedRectangle(
            shelf.withHeight(WorkspaceDock::headerHeight),
            CanvasChromeMetrics::controlCornerRadius);
    const Rectangle<float> minimize = minimizeButtonBounds(
            workspace, dockState, state);
    WorkspaceDock::paintIconButton(
            graphics,
            minimize,
            WorkspaceDockIcon::ChevronRight,
            focus.target == WorkspaceDockFocusTarget::GuideMinimize);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(FontOptions(CanvasChromeMetrics::labelFontSize));
    graphics.drawText(
            "Curve Guides",
            header.withTrimmedLeft(34.f).withWidth(96.f),
            Justification::centredLeft);
    const Rectangle<float> add = addButtonBounds(workspace, dockState, state);
    const auto addColours = CanvasChromePalette::control(
            focus.target == WorkspaceDockFocusTarget::GuideAdd
                    ? CanvasChromeControlState::Focused
                    : CanvasChromeControlState::Resting);
    graphics.setColour(addColours.surface);
    graphics.fillRoundedRectangle(add, CanvasChromeMetrics::controlCornerRadius);
    graphics.setColour(addColours.border);
    graphics.drawRoundedRectangle(
            add,
            CanvasChromeMetrics::controlCornerRadius,
            focus.target == WorkspaceDockFocusTarget::GuideAdd
                    ? CanvasChromeMetrics::focusRingWidth
                    : CanvasChromeMetrics::restingBorderWidth);
    graphics.setColour(addColours.text);
    graphics.setFont(FontOptions(CanvasChromeMetrics::captionFontSize));
    graphics.drawText("New", add, Justification::centred);

    if (shelf.getHeight() < WorkspaceDock::headerHeight
            + WorkspaceDock::guideTileHeight + WorkspaceDock::tileBottomPadding) {
        return;
    }

    if (graph.getGuideCurves().empty()) {
        const Rectangle<float> vacancy = WorkspaceDock::vacancyBounds(shelf);
        graphics.setColour(CanvasChromePalette::insetBackground.withAlpha(0.68f));
        graphics.fillRoundedRectangle(vacancy, CanvasChromeMetrics::tileCornerRadius);
        graphics.setColour(CanvasChromePalette::border.withAlpha(0.75f));
        graphics.drawRoundedRectangle(
                vacancy,
                CanvasChromeMetrics::tileCornerRadius,
                CanvasChromeMetrics::restingBorderWidth);
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.setFont(FontOptions(CanvasChromeMetrics::labelFontSize));
        graphics.drawText("No guides", vacancy.reduced(14.f), Justification::centredLeft);
        return;
    }

    Graphics::ScopedSaveState clip(graphics);
    graphics.reduceClipRegion(shelf.withTrimmedTop(WorkspaceDock::headerHeight).toNearestInt());
    for (int index = 0; index < (int) graph.getGuideCurves().size(); ++index) {
        const GuideCurveResource& guide = graph.getGuideCurves()[(size_t) index];
        const Rectangle<float> tile = tileBoundsFor(
                workspace,
                dockState,
                state,
                index);
        if (!tile.intersects(shelf)) {
            continue;
        }
        const bool selected = guide.id == state.selectedGuideId;
        const bool hovered = guide.id == state.hoveredGuideId;
        const bool focused = focus.target == WorkspaceDockFocusTarget::GuideTile
                && focus.itemId == guide.id;
        WorkspaceDock::paintTileChrome(
                graphics,
                tile,
                selected,
                hovered,
                focused);
        const Rectangle<float> thumbnail = previewBoundsFor(tile);
        graphics.setColour(CanvasChromePalette::canvasBackground.withAlpha(0.72f));
        graphics.fillRoundedRectangle(thumbnail, CanvasChromeMetrics::insetCornerRadius);
        Preview& preview = previewFor(guide, graph);
        preview.widget->paintPreviewSnapshot(graphics, thumbnail);
        if (hasDisplayName(guide)) {
            graphics.setColour(CanvasChromePalette::text);
            graphics.setFont(FontOptions(CanvasChromeMetrics::labelFontSize));
            graphics.drawText(
                    guide.name,
                    thumbnail.reduced(8.f).removeFromTop(22.f),
                    Justification::centredLeft);
        }
    }
    WorkspaceDock::paintVerticalOverflowFeedback(
            graphics,
            shelf,
            state.verticalOffset,
            maximumVerticalOffset(
                    workspace,
                    dockState,
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

int GuideCurveShelf::visiblePreviewCount(const NodeGraph& graph) const {
    int count = 0;
    for (const auto& guide : graph.getGuideCurves()) {
        const auto found = previews.find(guide.id);
        if (found != previews.end()
                && found->second.widget != nullptr
                && found->second.widget->hasVisiblePreviewSnapshot()) {
            ++count;
        }
    }
    return count;
}

bool GuideCurveShelf::renderOpenGL(
        const NodeGraph& graph,
        Rectangle<float> workspace,
        Rectangle<float> captureWorkspace,
        const SignalProbeRailState& dockState,
        const GuideCurveShelfState& state,
        float scaleFactor) {
    if (!dockState.expanded || state.minimized) {
        return false;
    }

    bool attempted {};
    const Rectangle<float> shelf = boundsFor(workspace, dockState, state);
    for (int index = 0; index < (int) graph.getGuideCurves().size(); ++index) {
        const GuideCurveResource& guide = graph.getGuideCurves()[(size_t) index];
        Preview& preview = previewFor(guide, graph);
        if (!preview.needsOpenGLRender) {
            continue;
        }

        const Rectangle<float> tile = tileBoundsFor(
                workspace,
                dockState,
                state,
                index);
        const Rectangle<float> thumbnail = previewBoundsFor(tile);
        const Rectangle<float> captureBounds(
                captureWorkspace.getX() + 4.f,
                captureWorkspace.getY() + 4.f,
                thumbnail.getWidth(),
                thumbnail.getHeight());
        const bool captured = preview.widget->renderGuidePreviewSnapshotOpenGL(
                captureBounds,
                scaleFactor);
        if (captured) {
            preview.needsOpenGLRender = false;
        }
        attempted = true;
    }
    return attempted;
}

void GuideCurveShelf::resetDocumentPreviews() {
    for (auto& entry : previews) {
        Preview& preview = entry.second;
        preview.widget->resetDocumentPresentation();
        preview.model.reset();
        preview.needsOpenGLRender = true;
    }
}

}
