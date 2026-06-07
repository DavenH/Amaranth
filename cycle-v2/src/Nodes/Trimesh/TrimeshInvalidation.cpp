#include "TrimeshInvalidation.h"

namespace CycleV2 {

namespace {

TrimeshInvalidationResult withPanelContextRefresh(
        TrimeshInvalidationResult result,
        const TrimeshChange& change) {
    if (change.gridShapeChanged || change.renderDomainChanged) {
        result.rebuildNodeData = true;
        result.updateRasterizer = true;
        result.refresh2DPanel = true;
        result.refresh3DGeometry = true;
        result.dirtyCompactPreview = true;
    }

    if (change.renderDomainChanged) {
        result.dirtyRenderProfile = true;
    }

    return result;
}

}

TrimeshInvalidationResult TrimeshInvalidation::invalidate(const TrimeshChange& change) const {
    TrimeshInvalidationResult result;

    switch (change.kind) {
        case TrimeshChangeKind::None:
            return withPanelContextRefresh(result, change);

        case TrimeshChangeKind::Morph:
            result.rebuildNodeData = true;
            result.updateRasterizer = true;
            result.refresh2DPanel = true;
            result.refresh3DGeometry = anyMorphChanged(change) && !primaryMorphChanged(change);
            result.dirtyCompactPreview = true;
            return withPanelContextRefresh(result, change);

        case TrimeshChangeKind::PrimaryAxis:
            result.rebuildNodeData = true;
            result.updateRasterizer = true;
            result.refresh2DPanel = true;
            result.refresh3DGeometry = true;
            result.dirtyCompactPreview = true;
            return withPanelContextRefresh(result, change);

        case TrimeshChangeKind::MeshEdit:
        case TrimeshChangeKind::VertexEdit:
            result.rebuildNodeData = true;
            result.updateRasterizer = true;
            result.refresh2DPanel = true;
            result.refresh3DGeometry = !change.sourceIs3D;
            result.dirtyCompactPreview = true;
            return withPanelContextRefresh(result, change);

        case TrimeshChangeKind::RenderProfile:
            result.dirtyRenderProfile = true;
            result.dirtyCompactPreview = true;
            return withPanelContextRefresh(result, change);

        case TrimeshChangeKind::Layout:
            result.rebuildNodeData = true;
            result.updateRasterizer = true;
            result.refresh2DPanel = true;
            result.refresh3DGeometry = true;
            result.dirtyCompactPreview = true;
            return withPanelContextRefresh(result, change);
    }

    return withPanelContextRefresh(result, change);
}

bool TrimeshInvalidation::primaryMorphChanged(const TrimeshChange& change) const {
    return (change.primaryViewAxis == Vertex::Time && change.yellowChanged)
        || (change.primaryViewAxis == Vertex::Red && change.redChanged)
        || (change.primaryViewAxis == Vertex::Blue && change.blueChanged);
}

bool TrimeshInvalidation::anyMorphChanged(const TrimeshChange& change) const {
    return change.yellowChanged || change.redChanged || change.blueChanged;
}

}
