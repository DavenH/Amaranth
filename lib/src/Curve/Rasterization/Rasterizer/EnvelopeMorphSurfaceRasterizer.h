#pragma once

#include <vector>
#include <optional>
#include <limits>

#include <Array/Column.h>
#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>

namespace Rasterization {

struct EnvelopeSurfaceMarkers {
    float loopStart { -1.f };
    float releaseStart { std::numeric_limits<float>::max() };
    float signalStart { std::numeric_limits<float>::max() };
    float signalEnd { -std::numeric_limits<float>::max() };
};

class EnvelopeMorphSurfaceRasterizer : public TrilinearMeshRasterizer {
public:
    EnvelopeMorphSurfaceRasterizer();

    void renderSurface(
            Mesh* mesh,
            std::vector<Column>& columns,
            int crossSectionResolution,
            int morphAxis,
            const MorphPosition& baseMorph,
            std::optional<Range<float>> timeRange = std::nullopt,
            std::vector<EnvelopeSurfaceMarkers>* markers = nullptr);
    void cleanUp();
    void reset() { cleanUp(); }

private:
    void renderCrossSectionAt(Mesh& mesh, const MorphPosition& morph);
};

}
