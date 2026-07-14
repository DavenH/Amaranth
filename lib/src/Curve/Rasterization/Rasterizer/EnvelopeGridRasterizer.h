#pragma once

#include <vector>

#include <Array/Column.h>
#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>

namespace Rasterization {

class EnvelopeGridRasterizer : public TrilinearMeshRasterizer {
public:
    EnvelopeGridRasterizer();

    void renderGrid(
            Mesh* mesh,
            std::vector<Column>& columns,
            int envelopeResolution,
            int dependentAxis);
    void cleanUp();
    void reset() { cleanUp(); }

private:
    void renderMesh(Mesh* mesh);
};

}
