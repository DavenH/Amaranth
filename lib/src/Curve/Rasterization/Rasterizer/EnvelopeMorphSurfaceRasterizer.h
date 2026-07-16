#pragma once

#include <vector>

#include <Array/Column.h>
#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>

namespace Rasterization {

class EnvelopeMorphSurfaceRasterizer : public TrilinearMeshRasterizer {
public:
    EnvelopeMorphSurfaceRasterizer();

    void renderSurface(
            Mesh* mesh,
            std::vector<Column>& columns,
            int crossSectionResolution,
            int morphAxis,
            const MorphPosition& baseMorph);
    void cleanUp();
    void reset() { cleanUp(); }

private:
    void renderCrossSectionAt(Mesh& mesh, const MorphPosition& morph);
};

}
