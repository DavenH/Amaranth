#include <catch2/catch_test_macros.hpp>

#include "../src/Nodes/Trimesh/TrimeshInvalidation.h"

using namespace CycleV2;

TEST_CASE("Trimesh invalidation skips 3D geometry for primary-axis morph edits", "[cycle-v2][nodes][trimesh]") {
    const TrimeshInvalidation invalidation;

    const TrimeshInvalidationResult result = invalidation.invalidate({
            TrimeshChangeKind::Morph,
            true,
            false,
            false,
            false,
            Vertex::Time
    });

    REQUIRE(result.rebuildNodeData);
    REQUIRE(result.updateRasterizer);
    REQUIRE(result.refresh2DPanel);
    REQUIRE_FALSE(result.refresh3DGeometry);
    REQUIRE(result.dirtyCompactPreview);
    REQUIRE(result.dirtySliceRasterization);
    REQUIRE(result.dirtyInterceptsRails);
    REQUIRE_FALSE(result.dirtyColumns3D);
    REQUIRE(result.dirtyDspPrep);
}

TEST_CASE("Trimesh invalidation refreshes 3D geometry for non-primary morph edits", "[cycle-v2][nodes][trimesh]") {
    const TrimeshInvalidation invalidation;

    const TrimeshInvalidationResult result = invalidation.invalidate({
            TrimeshChangeKind::Morph,
            false,
            true,
            false,
            false,
            Vertex::Time
    });

    REQUIRE(result.rebuildNodeData);
    REQUIRE(result.updateRasterizer);
    REQUIRE(result.refresh2DPanel);
    REQUIRE(result.refresh3DGeometry);
    REQUIRE(result.dirtyCompactPreview);
    REQUIRE(result.dirtySliceRasterization);
    REQUIRE(result.dirtyInterceptsRails);
    REQUIRE(result.dirtyColumns3D);
    REQUIRE(result.dirtyDspPrep);
}

TEST_CASE("Trimesh invalidation refreshes all panel geometry for primary-axis changes", "[cycle-v2][nodes][trimesh]") {
    const TrimeshInvalidation invalidation;

    const TrimeshInvalidationResult result = invalidation.invalidate({
            TrimeshChangeKind::PrimaryAxis,
            false,
            false,
            false,
            false,
            Vertex::Blue
    });

    REQUIRE(result.rebuildNodeData);
    REQUIRE(result.updateRasterizer);
    REQUIRE(result.refresh2DPanel);
    REQUIRE(result.refresh3DGeometry);
    REQUIRE(result.dirtyCompactPreview);
    REQUIRE(result.dirtySliceRasterization);
    REQUIRE(result.dirtyInterceptsRails);
    REQUIRE(result.dirtyColumns3D);
    REQUIRE(result.dirtyDspPrep);
}

TEST_CASE("Trimesh invalidation avoids redundant 3D refresh after 3D-sourced mesh edits", "[cycle-v2][nodes][trimesh]") {
    const TrimeshInvalidation invalidation;

    const TrimeshInvalidationResult result = invalidation.invalidate({
            TrimeshChangeKind::MeshEdit,
            false,
            false,
            false,
            true,
            Vertex::Time
    });

    REQUIRE(result.rebuildNodeData);
    REQUIRE(result.updateRasterizer);
    REQUIRE(result.refresh2DPanel);
    REQUIRE_FALSE(result.refresh3DGeometry);
    REQUIRE(result.dirtyCompactPreview);
    REQUIRE(result.dirtyMeshContent);
    REQUIRE(result.dirtySliceRasterization);
    REQUIRE(result.dirtyInterceptsRails);
    REQUIRE_FALSE(result.dirtyColumns3D);
    REQUIRE(result.dirtySelectedControl);
    REQUIRE(result.dirtyDspPrep);
}

TEST_CASE("Trimesh invalidation isolates selected-control changes", "[cycle-v2][nodes][trimesh]") {
    const TrimeshInvalidation invalidation;

    const TrimeshInvalidationResult result = invalidation.invalidate({
            TrimeshChangeKind::SelectedControl,
            false,
            false,
            false,
            false,
            Vertex::Time
    });

    REQUIRE_FALSE(result.rebuildNodeData);
    REQUIRE_FALSE(result.updateRasterizer);
    REQUIRE_FALSE(result.refresh2DPanel);
    REQUIRE_FALSE(result.refresh3DGeometry);
    REQUIRE_FALSE(result.dirtyCompactPreview);
    REQUIRE_FALSE(result.dirtyMeshContent);
    REQUIRE_FALSE(result.dirtySliceRasterization);
    REQUIRE_FALSE(result.dirtyInterceptsRails);
    REQUIRE_FALSE(result.dirtyColumns3D);
    REQUIRE(result.dirtySelectedControl);
    REQUIRE_FALSE(result.dirtyDspPrep);
}

TEST_CASE("Trimesh invalidation marks render profile changes without raster rebuild", "[cycle-v2][nodes][trimesh]") {
    const TrimeshInvalidation invalidation;

    const TrimeshInvalidationResult result = invalidation.invalidate({
            TrimeshChangeKind::RenderProfile,
            false,
            false,
            false,
            false,
            Vertex::Time
    });

    REQUIRE_FALSE(result.rebuildNodeData);
    REQUIRE_FALSE(result.updateRasterizer);
    REQUIRE_FALSE(result.refresh2DPanel);
    REQUIRE_FALSE(result.refresh3DGeometry);
    REQUIRE(result.dirtyCompactPreview);
    REQUIRE(result.dirtyRenderProfile);
    REQUIRE_FALSE(result.dirtySliceRasterization);
    REQUIRE_FALSE(result.dirtyInterceptsRails);
    REQUIRE_FALSE(result.dirtyColumns3D);
    REQUIRE_FALSE(result.dirtyDspPrep);
}

TEST_CASE("Trimesh invalidation promotes panel context changes to raster refresh", "[cycle-v2][nodes][trimesh]") {
    const TrimeshInvalidation invalidation;

    const TrimeshInvalidationResult result = invalidation.invalidate({
            TrimeshChangeKind::Morph,
            true,
            false,
            false,
            false,
            Vertex::Time,
            true,
            true
    });

    REQUIRE(result.rebuildNodeData);
    REQUIRE(result.updateRasterizer);
    REQUIRE(result.refresh2DPanel);
    REQUIRE(result.refresh3DGeometry);
    REQUIRE(result.dirtyCompactPreview);
    REQUIRE(result.dirtyRenderProfile);
    REQUIRE(result.dirtySliceRasterization);
    REQUIRE(result.dirtyInterceptsRails);
    REQUIRE(result.dirtyColumns3D);
}
