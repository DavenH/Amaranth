#pragma once

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
    int rows {};
    int columns {};

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

class TrimeshNodeModel {
public:
    TrimeshNodeModel();
    ~TrimeshNodeModel();

    TrimeshNodeModel(TrimeshNodeModel&&) noexcept;
    TrimeshNodeModel& operator=(TrimeshNodeModel&&) noexcept;

    TrimeshNodeModel(const TrimeshNodeModel&) = delete;
    TrimeshNodeModel& operator=(const TrimeshNodeModel&) = delete;

    void syncFromNode(const Node& node);

    TrimeshRenderData renderGrid(int rows, int columns);
    std::vector<TrimeshVertexParameter> getSelectedVertexParameters();
    std::vector<TrimeshVertexMarker> getVertexMarkers();
    int findNearestVertexIndexForPhaseAmp(float phase, float amp);
    int getResolvedSelectedVertexIndex();
    void markMeshEdited();

    const MorphPosition& getMorphPosition() const { return morph; }
    int getPrimaryViewAxis() const { return primaryViewAxis; }
    int getSelectedVertexIndex() const { return selectedVertexIndex; }
    uint64_t getRevision() const { return revision; }
    Mesh& getMeshForPanel() { return mesh(); }

private:
    Mesh& mesh();
    int resolvedSelectedVertexIndex();
    Vertex* selectedVertex();
    bool applyVertexParameterOverrides(const Node& node);
    void clearMesh();

    std::unique_ptr<Mesh> ownedMesh;
    MorphPosition morph { 0.5f, 0.5f, 0.5f };
    int primaryViewAxis {};
    int selectedVertexIndex { -1 };
    uint64_t revision {};
};

}
