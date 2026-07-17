#pragma once

#include "TrimeshRenderProfile.h"

#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>

struct Intercept;

namespace CycleV2 {

class TrimeshInteractor2D;
class TrimeshInteractor3D;
class TrimeshNodeModel;
class TrimeshPanel2D;
class TrimeshPanel3D;
class TrimeshPanelHosts;

class TrimeshPanelRasterizer {
public:
    Rasterization::TrilinearMeshRasterizer& getRasterizer() { return rasterizer; }
    bool wrapsVertices() { return rasterizer.snapshotView().wrapsVertices(); }

    void update(
            TrimeshNodeModel& model,
            TrimeshRenderProfile renderProfile,
            TrimeshInteractor2D& interactor2D,
            TrimeshInteractor3D& interactor3D,
            TrimeshPanel2D& panel2D,
            TrimeshPanel3D& panel3D,
            TrimeshPanelHosts& panelHosts,
            bool refresh2DPanel,
            bool refresh3DGeometry);

private:
    Rasterization::TrilinearMeshRasterizer rasterizer;
};

}
