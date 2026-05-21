#include "TrimeshPanelBridge.h"

#include <App/AppConstants.h>
#include <App/MeshLibrary.h>
#include <Curve/Mesh/Vertex.h>
#include <UI/MiscGraphics.h>
#include <UI/Panels/OpenGLPanel.h>
#include <UI/Panels/OpenGLPanel3D.h>

namespace CycleV2 {

TrimeshPanelBridge::TrimeshPanelBridge() :
        console         (&repo)
    ,   interactor2D    (&repo, "CycleV2TrimeshInteractor2D",
                         Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Time, Vertex::Red, Vertex::Blue))
    ,   interactor3D    (&repo, "CycleV2TrimeshInteractor3D")
    ,   panel2D         (&repo)
    ,   panel3D         (&repo, dataSource) {
    repo.add(new AppConstants(&repo));
    repo.add(new MiscGraphics(&repo));
    repo.add(new Settings(&repo));
    repo.add(new MeshLibrary(&repo));

    auto& constants = repo.get<AppConstants>("AppConstants");
    constants.setConstant(Constants::FontFace, String("Verdana"));

    repo.get<MiscGraphics>("MiscGraphics").init();

    auto& settings = repo.get<Settings>("Settings");
    settings.initialiseSettings();
    settings.createPropertiesFile(
            File::getSpecialLocation(File::tempDirectory)
                    .getChildFile("cycle-v2-trimesh-bridge-settings.xml")
                    .getFullPathName());
    settings.getGlobalSetting(AppSettings::DrawScales) = false;
    repo.setConsole(&console);
    repo.setMorphPositioner(&morphPositioner);

    interactor2D.setRasterizer(&rasterizer);
    interactor3D.setRasterizer(&rasterizer);
    panel2D.setInteractor(&interactor2D);
    panel3D.setInteractor(&interactor3D);
}

TrimeshPanelBridge::~TrimeshPanelBridge() {
    interactor2D.stopTimer();
    interactor3D.stopTimer();
    panel2D.setInteractor(nullptr);
    panel3D.setInteractor(nullptr);
    interactor2D.setRasterizer(nullptr);
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
    request.dims = interactor2D.dims;
    request.scalingMode = Rasterization::PointScalingMode::Bipolar;
    request.calcDepthDimensions = true;
    request.lowResCurves = false;
    request.batchMode = true;

    rasterizer.setWrapsEnds(true);
    rasterizer.setMesh(&model.getMeshForPanel());
    rasterizer.updateWaveform();
}

Component* TrimeshPanelBridge::getPanel3DComponent() {
    if (!panelInitialised) {
        panel3D.init();
        panelInitialised = true;
    }

    return panel3D.getOpenglPanel();
}

Component* TrimeshPanelBridge::getPanel3DComponentIfCreated() {
    return panelInitialised ? panel3D.getOpenglPanel() : nullptr;
}

Component* TrimeshPanelBridge::getPanel2DComponent() {
    if (!panel2DInitialised) {
        panel2D.init();
        panel2DInitialised = true;
    }

    return panel2D.getOpenglPanel();
}

Component* TrimeshPanelBridge::getPanel2DComponentIfCreated() {
    return panel2DInitialised ? panel2D.getOpenglPanel() : nullptr;
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
