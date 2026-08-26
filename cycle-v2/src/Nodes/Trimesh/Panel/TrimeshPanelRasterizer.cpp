#include "Nodes/Trimesh/Panel/TrimeshPanelRasterizer.h"

#include "Nodes/Trimesh/Panel/TrimeshInteractor2D.h"
#include "Nodes/Trimesh/Panel/TrimeshInteractor3D.h"
#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"
#include "Nodes/Trimesh/Panel/TrimeshPanel2D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanel3D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelHosts.h"

namespace CycleV2 {

void TrimeshPanelRasterizer::update(
        TrimeshNodeModel& model,
        TrimeshRenderProfile renderProfile,
        TrimeshInteractor2D& interactor2D,
        TrimeshInteractor3D& interactor3D,
        TrimeshPanel2D& panel2D,
        TrimeshPanel3D& panel3D,
        TrimeshPanelHosts& panelHosts,
        bool refresh2DPanel,
        bool refresh3DGeometry) {
    const auto& curveStyle = renderProfile.getCurveStyle();
    const bool cyclic = curveStyle.cyclic;
    Rasterization::RasterizationRequest request;
    request.cyclic = cyclic;
    request.xMinimum = curveStyle.xMinimum;
    request.xMaximum = curveStyle.xMaximum;
    request.morph = model.getMorphPosition();
    request.primaryViewDimension = model.getPrimaryViewAxis();
    request.dims = interactor2D.dims;
    request.scalingMode = Rasterization::PointScalingMode::Unipolar;
    request.calcDepthDimensions = true;
    request.lowResCurves = false;
    auto& mesh = model.getMeshForPanel();
    interactor2D.setMesh(&model.getMeshForPanel());
    interactor3D.setMesh(&model.getMeshForPanel());
    const auto& result = rasterizer.renderWaveform({ mesh, request, 0.f });
    rasterizer.publish(result, { cyclic });

    if (panelHosts.isPanel3DHostInitialised() && refresh3DGeometry) {
        interactor3D.updateIntercepts();
        panel3D.requestRepaint();
    }

    if (refresh2DPanel && panelHosts.isPanel2DHostInitialised()) {
        panel2D.requestRepaint();
    }
}

}
