#include "TrimeshPanelBridge.h"

#include <Curve/Mesh/Vertex.h>
#include <UI/Panels/OpenGLPanel3D.h>

namespace CycleV2 {

TrimeshPanelBridge::TrimeshPanelBridge() :
        interactor3D    (&repo, "CycleV2TrimeshInteractor3D")
    ,   panel3D         (&repo, dataSource) {
    repo.setMorphPositioner(&morphPositioner);
    interactor3D.setRasterizer(&rasterizer);
    panel3D.setInteractor(&interactor3D);
}

TrimeshPanelBridge::~TrimeshPanelBridge() {
    interactor3D.stopTimer();
    panel3D.setInteractor(nullptr);
    interactor3D.setRasterizer(nullptr);
}

void TrimeshPanelBridge::syncFromNode(
        const Node& node,
        int rows,
        int columns) {
    model.syncFromNode(node);
    morphPositioner.setPosition(model.getMorphPosition(), model.getPrimaryViewAxis());

    if (lastSyncedRevision == model.getRevision()
            && lastRows == rows
            && lastColumns == columns) {
        return;
    }

    dataSource.rebuild(model, rows, columns);
    updateRasterizer();
    lastSyncedRevision = model.getRevision();
    lastRows = rows;
    lastColumns = columns;
}

void TrimeshPanelBridge::updateRasterizer() {
    auto& request = rasterizer.getRequest();
    request.cyclic = true;
    request.xMinimum = -0.05f;
    request.xMaximum = 1.05f;
    request.morph = model.getMorphPosition();
    request.primaryViewDimension = model.getPrimaryViewAxis();
    request.scalingMode = Rasterization::PointScalingMode::Bipolar;
    request.calcDepthDimensions = true;
    request.lowResCurves = false;
    request.batchMode = true;

    rasterizer.setWrapsEnds(true);
    rasterizer.setMesh(&model.getMeshForPanel());
    rasterizer.updateGeometry();
}

Component* TrimeshPanelBridge::getPanelComponent() {
    if (!panelInitialised) {
        panel3D.init();
        panelInitialised = true;
    }

    return panel3D.getOpenglPanel();
}

Component* TrimeshPanelBridge::getPanelComponentIfCreated() {
    return panelInitialised ? panel3D.getOpenglPanel() : nullptr;
}

void TrimeshPanelBridge::NodeMorphPositioner::setPosition(
        const MorphPosition& position,
        int primaryAxis) {
    morph = position;
    primaryDimension = primaryAxis;
}

float TrimeshPanelBridge::NodeMorphPositioner::getValue(int dim) {
    switch (dim) {
        case Vertex::Time: return morph.time.getCurrentValue();
        case Vertex::Red:  return morph.red.getCurrentValue();
        case Vertex::Blue: return morph.blue.getCurrentValue();
        default:           return 0.f;
    }
}

}
