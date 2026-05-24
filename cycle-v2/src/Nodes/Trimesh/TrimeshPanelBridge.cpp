#include "TrimeshPanelBridge.h"

#include <App/AppConstants.h>
#include <App/MeshLibrary.h>
#include <Curve/Mesh/Vertex.h>
#include <UI/MiscGraphics.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>
#include <UI/Panels/PanelHostContext.h>

namespace CycleV2 {

namespace {

class PanelHostComponent :
        public Component {
public:
    explicit PanelHostComponent(Panel& targetPanel) :
            panel(targetPanel) {
        setPaintingIsUnclipped(true);
        setOpaque(false);
    }

    void paint(Graphics&) override {}

    void resized() override {
        panel.panelResized();
    }

private:
    Panel& panel;
};

}

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

    interactor2D.init();
    interactor3D.init();
    interactor2D.setRasterizer(&rasterizer);
    interactor3D.setRasterizer(&rasterizer);
    interactor2D.setMeshEditedCallback([this](bool sourceIs3D) { refreshAfterMeshEdit(sourceIs3D); });
    interactor3D.setMeshEditedCallback([this](bool sourceIs3D) { refreshAfterMeshEdit(sourceIs3D); });
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
    releaseSharedGlResources();
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
    updateRasterizer(true, false);
    lastSyncedRevision = model.getRevision();
    lastRows = rows;
    lastColumns = columns;
}

void TrimeshPanelBridge::refreshAfterMeshEdit(bool sourceIs3D) {
    model.markMeshEdited();
    dataSource.rebuild(model, lastRows, lastColumns);
    updateRasterizer(sourceIs3D, !sourceIs3D);
}

void TrimeshPanelBridge::updateRasterizer(bool refresh2DPanel, bool refresh3DGeometry) {
    auto& request = rasterizer.getRequest();
    request.cyclic = true;
    request.xMinimum = -0.05f;
    request.xMaximum = 1.05f;
    request.morph = model.getMorphPosition();
    request.primaryViewDimension = model.getPrimaryViewAxis();
    request.dims = interactor2D.dims;
    request.scalingMode = Rasterization::PointScalingMode::Unipolar;
    request.calcDepthDimensions = true;
    request.lowResCurves = false;
    request.batchMode = true;

    rasterizer.setWrapsEnds(true);
    rasterizer.setMesh(&model.getMeshForPanel());
    interactor2D.setMesh(&model.getMeshForPanel());
    interactor3D.setMesh(&model.getMeshForPanel());
    rasterizer.updateWaveform();

    if (panel3DHostInitialised) {
        if (refresh3DGeometry) {
            interactor3D.updateIntercepts();
        }

        panel3D.bakeTexturesNextRepaint();
        panel3D.requestRepaint();
    }

    if (refresh2DPanel && panel2DHostInitialised) {
        interactor2D.performUpdate(Update);
        panel2D.requestRepaint();
    }
}

Component* TrimeshPanelBridge::getPanel3DHostComponent() {
    initialisePanel3DHost();
    return panel3DHost.get();
}

Component* TrimeshPanelBridge::getPanel3DHostComponentIfCreated() {
    return panel3DHostInitialised ? panel3DHost.get() : nullptr;
}

Component* TrimeshPanelBridge::getPanel2DHostComponent() {
    initialisePanel2DHost();
    return panel2DHost.get();
}

Component* TrimeshPanelBridge::getPanel2DHostComponentIfCreated() {
    return panel2DHostInitialised ? panel2DHost.get() : nullptr;
}

void TrimeshPanelBridge::initialisePanel3DHost() {
    if (panel3DHostInitialised) {
        return;
    }

    panel3DHost = std::make_unique<PanelHostComponent>(panel3D);
    panel3D.setSharedCanvasMode(true);
    panel3D.initWithExternalComponent(panel3DHost.get());
    panel3DHostInitialised = true;
}

void TrimeshPanelBridge::initialisePanel2DHost() {
    if (panel2DHostInitialised) {
        return;
    }

    panel2DHost = std::make_unique<PanelHostComponent>(panel2D);
    panel2D.initWithExternalComponent(panel2DHost.get());
    panel2DHostInitialised = true;
}

void TrimeshPanelBridge::initialiseSharedGlResources() {
    initialisePanel3DHost();
    initialisePanel2DHost();

    if (sharedGlResourcesInitialised) {
        return;
    }

    panel3DGfx = new CommonGL(&panel3D);
    panel3DRenderer = std::make_unique<GLPanelRenderer>(panel3DGfx);
    panel3D.setGraphicsHelper(panel3DGfx);
    panel3D.setPanelRenderer(panel3DRenderer.get());
    panel3DGfx->initializeTextures();

    panel2DGfx = new CommonGL(&panel2D);
    panel2DRenderer = std::make_unique<GLPanelRenderer>(panel2DGfx);
    panel2D.setGraphicsHelper(panel2DGfx);
    panel2D.setPanelRenderer(panel2DRenderer.get());
    panel2DGfx->initializeTextures();

    panel3D.bakeTexturesNextRepaint();
    panel2D.bakeTexturesNextRepaint();
    sharedGlResourcesInitialised = true;
}

void TrimeshPanelBridge::releaseSharedGlResources() {
    panel2D.setPanelRenderer(nullptr);
    panel3D.setPanelRenderer(nullptr);
    panel2DRenderer = nullptr;
    panel3DRenderer = nullptr;
    panel2D.setGraphicsHelper(nullptr);
    panel3D.setGraphicsHelper(nullptr);
    panel2DGfx = nullptr;
    panel3DGfx = nullptr;
    sharedGlResourcesInitialised = false;
}

void TrimeshPanelBridge::renderPanel3D(Rectangle<float> bounds, float scaleFactor) {
    initialiseSharedGlResources();
    renderPanel(panel3D, bounds, scaleFactor);
}

void TrimeshPanelBridge::renderPanel2D(Rectangle<float> bounds, float scaleFactor) {
    initialiseSharedGlResources();
    renderPanel(panel2D, bounds, scaleFactor);
}

void TrimeshPanelBridge::renderPanel(Panel& panel, Rectangle<float> bounds, float scaleFactor) {
    if (bounds.isEmpty()) {
        return;
    }

    PanelHostContext context;
    context.bounds = bounds;
    context.clip = bounds;
    context.scaleFactor = scaleFactor;
    context.visible = true;
    panel.render(context);
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
