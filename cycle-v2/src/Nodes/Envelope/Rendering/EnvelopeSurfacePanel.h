#pragma once

#include "Nodes/Trimesh/Panel/TrimeshPanel3D.h"

#include <Curve/Rasterization/Rasterizer/EnvelopeMorphSurfaceRasterizer.h>

namespace CycleV2 {

class EnvelopeSurfacePanel final : public TrimeshPanel3D {
public:
    EnvelopeSurfacePanel(SingletonRepo* repo, Panel3D::DataRetriever& source);

    static Image createColourMap(const std::vector<float>& values, int columns, int rows,
            Range<float> timeRange, const std::vector<Rasterization::EnvelopeSurfaceMarkers>& markers, bool bipolar = false);
    void setSurfaceImage(Image image, Range<float> timeRange);
    void drawCurvesAndSurfaces() override;
    bool usesCachedSurface() const override { return false; }
    uint64_t textureUploadCount() const { return uploads; }

private:
    Range<float> surfaceTimeRange { 0.f, 1.f };
    uint64_t uploads {};
};

}
