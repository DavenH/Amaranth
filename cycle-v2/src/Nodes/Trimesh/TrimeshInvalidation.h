#pragma once

#include <Curve/Mesh/Vertex.h>

namespace CycleV2 {

enum class TrimeshChangeKind {
    None,
    Morph,
    PrimaryAxis,
    MeshEdit,
    VertexEdit,
    SelectedControl,
    RenderProfile,
    Layout
};

struct TrimeshChange {
    TrimeshChangeKind kind { TrimeshChangeKind::None };
    bool yellowChanged {};
    bool redChanged {};
    bool blueChanged {};
    bool sourceIs3D {};
    int primaryViewAxis { Vertex::Time };
    bool gridShapeChanged {};
    bool renderDomainChanged {};
};

struct TrimeshInvalidationResult {
    bool rebuildNodeData {};
    bool updateRasterizer {};
    bool refresh2DPanel {};
    bool refresh3DGeometry {};
    bool dirtyCompactPreview {};
    bool dirtyRenderProfile {};
    bool dirtyMeshContent {};
    bool dirtySliceRasterization {};
    bool dirtyInterceptsRails {};
    bool dirtyColumns3D {};
    bool dirtySelectedControl {};
    bool dirtyDspPrep {};
};

class TrimeshInvalidation {
public:
    TrimeshInvalidationResult invalidate(const TrimeshChange& change) const;

private:
    bool primaryMorphChanged(const TrimeshChange& change) const;
    bool anyMorphChanged(const TrimeshChange& change) const;
};

}
