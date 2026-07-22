#pragma once

#include "TrimeshMeshState.h"

#include "../../Graph/NodeGraph.h"

#include <Obj/MorphPosition.h>

#include <cstdint>
#include <memory>
#include <vector>

class Mesh;
class Vertex;

namespace CycleV2 {

struct TrimeshRenderData {
    std::vector<float> surface;
    std::vector<float> slice;
    PortDomain domain { PortDomain::TimeSignal };
    int rows {};
    int columns {};
    bool cyclic { true };

    bool canDrawSurface() const {
        return rows >= 2 && columns >= 2 && surface.size() >= (size_t) rows * (size_t) columns;
    }
};

struct TrimeshVertexParameter {
    String id;
    String label;
    float value {};
    float minimum {};
    float maximum { 1.f };
};

struct TrimeshVertexMarker {
    int index { -1 };
    float phase {};
    float amp {};
    bool selected {};
};

struct TrimeshCubePreviewVertex {
    float time {};
    float red {};
    float blue {};
    bool selected {};
};

struct TrimeshDerivedRevisions {
    uint64_t meshContent {};
    uint64_t sliceRasterization {};
    uint64_t interceptsRails {};
    uint64_t columns3D {};
    uint64_t compactPreview {};
    uint64_t selectedControl {};
    uint64_t dspPrep {};
    uint64_t aggregate {};
};

enum class TrimeshDerivedProduct : uint32_t {
    None = 0,
    MeshContent = 1 << 0,
    SliceRasterization = 1 << 1,
    InterceptsRails = 1 << 2,
    Columns3D = 1 << 3,
    CompactPreview = 1 << 4,
    SelectedControl = 1 << 5,
    DspPreparation = 1 << 6
};

constexpr TrimeshDerivedProduct operator|(
        TrimeshDerivedProduct left,
        TrimeshDerivedProduct right) {
    return (TrimeshDerivedProduct) ((uint32_t) left | (uint32_t) right);
}

class TrimeshNodeModel {
public:
    TrimeshNodeModel();
    ~TrimeshNodeModel();

    TrimeshNodeModel(TrimeshNodeModel&&) noexcept;
    TrimeshNodeModel& operator=(TrimeshNodeModel&&) noexcept;

    TrimeshNodeModel(const TrimeshNodeModel&) = delete;
    TrimeshNodeModel& operator=(const TrimeshNodeModel&) = delete;

    void syncFromNode(const Node& node);

    TrimeshRenderData renderGrid(int rows, int columns, PortDomain domain = PortDomain::TimeSignal);
    std::vector<TrimeshVertexParameter> getVertexParametersForIndex(int vertexIndex);
    std::vector<TrimeshVertexParameter> getSelectedVertexParameters();
    std::vector<TrimeshVertexMarker> getVertexMarkers();
    std::vector<TrimeshCubePreviewVertex> getSelectedCubePreviewVertices();
    int findNearestVertexIndexForPhaseAmp(float phase, float amp);
    int getResolvedSelectedVertexIndex();
    void selectVertex(Vertex* vertex);
    bool setVertexParameter(int vertexIndex, const String& parameterId, float value);
    void markMeshEdited();

    const MorphPosition& getMorphPosition() const { return morph; }
    int getPrimaryViewAxis() const { return primaryViewAxis; }
    int getSelectedVertexIndex() const { return selectedVertexIndex; }
    uint64_t getRevision() const { return revision; }
    const TrimeshDerivedRevisions& getDerivedRevisions() const { return revisions; }
    Mesh& getMeshForPanel() { return mesh(); }
    var currentMeshJSON();

private:
    Mesh& mesh();
    int resolvedSelectedVertexIndex();
    Vertex* vertexAtIndex(int vertexIndex);
    Vertex* selectedVertex();
    static int vertexValueIndex(const String& parameterId);
    void bumpMeshContentRevision();
    void bumpMorphRevision();
    void bumpPrimaryAxisRevision();
    void bumpSelectedControlRevision();
    void advanceDerivedRevisions(TrimeshDerivedProduct products);
    void clearMesh();

    std::unique_ptr<Mesh> ownedMesh;
    MorphPosition morph { 0.5f, 0.5f, 0.5f };
    int primaryViewAxis {};
    int selectedVertexIndex { -1 };
    uint64_t revision {};
    uint64_t appliedModelRevision {};
    TrimeshDerivedRevisions revisions;
};

}
