#include "GuideCurveShelf.h"

#include "SignalProbeRail.h"

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

}

Rectangle<float> GuideCurveShelf::guideWorkspace(Rectangle<float> workspace, float splitRatio) {
    const float ratio = jlimit(0.2f, 0.8f, splitRatio);
    return workspace.removeFromLeft(workspace.getWidth() * ratio);
}

Rectangle<float> GuideCurveShelf::spyWorkspace(Rectangle<float> workspace, float splitRatio) {
    const float ratio = jlimit(0.2f, 0.8f, splitRatio);
    workspace.removeFromLeft(workspace.getWidth() * ratio);
    return workspace;
}

Rectangle<float> GuideCurveShelf::boundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& dockState,
        float splitRatio,
        const GuideCurveShelfState& state) {
    Rectangle<float> guideWorkspaceBounds = guideWorkspace(workspace, splitRatio);
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
    graphics.drawText("−", minimize, Justification::centred);
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
        graphics.drawText("◇", vacancy.removeFromTop(28.f), Justification::centred);
        graphics.drawText("No Guide Curves", vacancy, Justification::centred);
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
        graphics.setColour(guide.id == state.selectedGuideId ? Colour(0xff8ac4ff) : kShelfBorder);
        graphics.drawRoundedRectangle(tile, 7.f, guide.id == state.selectedGuideId ? 2.f : 1.f);
        graphics.setColour(kText);
        graphics.setFont(FontOptions(12.f, Font::bold));
        graphics.drawText(guide.shortLabel, tile.reduced(10.f).removeFromTop(24.f), Justification::centredLeft);
        graphics.setColour(kMutedText);
        graphics.setFont(FontOptions(11.f));
        graphics.drawText(
                guide.name.isEmpty() ? "Guide Curve" : guide.name,
                tile.reduced(10.f).withTrimmedTop(28.f),
                Justification::centredLeft);
    }
}

}
