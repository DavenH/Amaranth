#include "TrimeshPanelRasterizer.h"

#include "TrimeshInteractor2D.h"
#include "TrimeshInteractor3D.h"
#include "TrimeshNodeModel.h"
#include "TrimeshPanel2D.h"
#include "TrimeshPanel3D.h"
#include "TrimeshPanelHosts.h"

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
    auto& request = rasterizer.getRequest();
    request.cyclic = cyclic;
    request.xMinimum = curveStyle.xMinimum;
    request.xMaximum = curveStyle.xMaximum;
    request.morph = model.getMorphPosition();
    request.primaryViewDimension = model.getPrimaryViewAxis();
    request.dims = interactor2D.dims;
    request.scalingMode = Rasterization::PointScalingMode::Unipolar;
    request.calcDepthDimensions = true;
    request.lowResCurves = false;
    rasterizer.setWrapsEnds(cyclic);
    rasterizer.setMesh(&model.getMeshForPanel());
    interactor2D.setMesh(&model.getMeshForPanel());
    interactor3D.setMesh(&model.getMeshForPanel());
    rasterizer.updateWaveform();

    if (panelHosts.isPanel3DHostInitialised() && refresh3DGeometry) {
        interactor3D.updateIntercepts();
        panel3D.bakeTexturesNextRepaint();
        panel3D.requestRepaint();
    }

    if (refresh2DPanel && panelHosts.isPanel2DHostInitialised()) {
        interactor2D.performUpdate(Update);
        panel2D.requestRepaint();
    }
}

}
