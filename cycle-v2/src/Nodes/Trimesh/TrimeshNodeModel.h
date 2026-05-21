#pragma once

#include "../../Graph/NodeGraph.h"

#include <Obj/MorphPosition.h>

#include <cstdint>
#include <memory>
#include <vector>

class Mesh;

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

    const MorphPosition& getMorphPosition() const { return morph; }
    int getPrimaryViewAxis() const { return primaryViewAxis; }
    uint64_t getRevision() const { return revision; }

private:
    Mesh& mesh();
    void clearMesh();

    std::unique_ptr<Mesh> ownedMesh;
    MorphPosition morph { 0.5f, 0.5f, 0.5f };
    int primaryViewAxis {};
    uint64_t revision {};
};

}
