#include "TrimeshPanelBridge.h"

#include <Curve/Mesh/Vertex.h>

namespace CycleV2 {

namespace {

uint64_t panelRevisionFor(const TrimeshNodeModel& model) {
    const TrimeshDerivedRevisions& revisions = model.getDerivedRevisions();
    return jmax(
            revisions.sliceRasterization,
            revisions.interceptsRails,
            revisions.columns3D);
}

}

TrimeshPanelBridge::TrimeshPanelBridge() :
        interactor2D    (&environment.getRepo(), "CycleV2TrimeshInteractor2D",
                         Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Time, Vertex::Red, Vertex::Blue))
    ,   interactor3D    (&environment.getRepo(), "CycleV2TrimeshInteractor3D")
    ,   panel2D         (&environment.getRepo())
    ,   panel3D         (&environment.getRepo(), dataSource)
    ,   panelHosts      (panel2D, panel3D, interactor2D, interactor3D) {

    interactor2D.init();
    interactor3D.init();
    interactor2D.stopTimer();
    interactor3D.stopTimer();
    interactor2D.setRasterizer(&panelRasterizer.getRasterizer());
    interactor3D.setRasterizer(&panelRasterizer.getRasterizer());
    interactor2D.setRasterizerUpdatesEnabled(false);
    interactor3D.setRasterizerUpdatesEnabled(false);
    interactor2D.setMeshEditedCallback([this](TrimeshMeshEditEvent event) {
        refreshAfterMeshEdit(event);
    });
    interactor3D.setMeshEditedCallback([this](TrimeshMeshEditEvent event) {
        refreshAfterMeshEdit(event);
    });
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
    const uint64_t previousPanelRevision = panelRevisionFor(model);
    const int previousPrimaryAxis = model.getPrimaryViewAxis();
    const MorphPosition previousMorph = model.getMorphPosition();

    model.syncFromNode(node);
    environment.setMorphPosition(model.getMorphPosition(), model.getPrimaryViewAxis());
    syncPrimaryAxisContext();

    const uint64_t nextPanelRevision = panelRevisionFor(model);
    const bool panelDataChanged = previousPanelRevision != nextPanelRevision;
    const bool primaryAxisChanged = previousPrimaryAxis != model.getPrimaryViewAxis();
    const MorphPosition& nextMorph = model.getMorphPosition();
    const bool yellowChanged = previousMorph.time.getTargetValue() != nextMorph.time.getTargetValue();
    const bool redChanged = previousMorph.red.getTargetValue() != nextMorph.red.getTargetValue();
    const bool blueChanged = previousMorph.blue.getTargetValue() != nextMorph.blue.getTargetValue();
    const bool morphChanged = yellowChanged || redChanged || blueChanged;
    const bool renderDomainChanged = lastRenderDomain != renderProfile.getDomain();
    const bool gridShapeChanged = lastRows != rows || lastColumns != columns;

    if (!panelDataChanged
            && !morphChanged
            && !primaryAxisChanged
            && !renderDomainChanged
            && lastSyncedRevision == nextPanelRevision
            && lastRows == rows
            && lastColumns == columns) {
        return;
    }

    TrimeshChange change;
    change.kind = primaryAxisChanged
            ? TrimeshChangeKind::PrimaryAxis
            : (morphChanged ? TrimeshChangeKind::Morph : TrimeshChangeKind::Layout);
    change.yellowChanged = yellowChanged;
    change.redChanged = redChanged;
    change.blueChanged = blueChanged;
    change.primaryViewAxis = model.getPrimaryViewAxis();
    change.gridShapeChanged = gridShapeChanged;
    change.renderDomainChanged = renderDomainChanged;

    const TrimeshInvalidationResult invalidated = invalidation.invalidate(change);
    dataSource.rebuild(model, rows, columns, renderProfile.getDomain());
    updateRasterizer(invalidated.refresh2DPanel, invalidated.refresh3DGeometry);
    lastSyncedRevision = nextPanelRevision;
    lastRenderDomain = renderProfile.getDomain();
    lastRows = rows;
    lastColumns = columns;
}

void TrimeshPanelBridge::refreshAfterMeshEdit(TrimeshMeshEditEvent event) {
    const TrimeshInvalidationResult invalidated = invalidation.invalidate({
            TrimeshChangeKind::MeshEdit,
            false,
            false,
            false,
            event.sourceIs3D,
            model.getPrimaryViewAxis()
    });

    vector<Vertex*>& selected = event.sourceIs3D
            ? interactor3D.getSelected()
            : interactor2D.getSelected();
    if (!selected.empty()) {
        model.selectVertex(selected.front());
    }
    model.markMeshEdited();
    syncPrimaryAxisContext();

    if (event.gestureComplete && meshEditedCallback != nullptr) {
        meshEditedCallback();
    }

    dataSource.rebuild(model, lastRows, lastColumns, renderProfile.getDomain());
    updateRasterizer(invalidated.refresh2DPanel, invalidated.refresh3DGeometry);
    lastSyncedRevision = panelRevisionFor(model);
}

void TrimeshPanelBridge::setMeshEditedCallback(std::function<void()> callback) {
    meshEditedCallback = std::move(callback);
}

int TrimeshPanelBridge::selectedVertexIndexForPanel() {
    vector<Vertex*>& selected2D = interactor2D.getSelected();
    vector<Vertex*>& selected3D = interactor3D.getSelected();
    Vertex* selected = !selected2D.empty()
            ? selected2D.front()
            : (!selected3D.empty() ? selected3D.front() : nullptr);
    const auto& vertices = model.getMeshForPanel().getVerts();
    for (int i = 0; i < (int) vertices.size(); ++i) {
        if (vertices[(size_t) i] == selected) {
            return i;
        }
    }
    return model.getResolvedSelectedVertexIndex();
}

void TrimeshPanelBridge::syncPrimaryAxisContext() {
    interactor3D.setPrimaryViewAxis(model.getPrimaryViewAxis());
    panel3D.setPrimaryViewAxis(model.getPrimaryViewAxis());
}

void TrimeshPanelBridge::updateRasterizer(bool refresh2DPanel, bool refresh3DGeometry) {
    panelRasterizer.update(
            model,
            renderProfile,
            interactor2D,
            interactor3D,
            panel2D,
            panel3D,
            panelHosts,
            refresh2DPanel,
            refresh3DGeometry);
}

Component* TrimeshPanelBridge::getPanel3DHostComponent() {
    return panelHosts.getPanel3DHostComponent();
}

Component* TrimeshPanelBridge::getPanel3DHostComponentIfCreated() const {
    return panelHosts.getPanel3DHostComponentIfCreated();
}

Component* TrimeshPanelBridge::getPanel2DHostComponent() {
    return panelHosts.getPanel2DHostComponent();
}

Component* TrimeshPanelBridge::getPanel2DHostComponentIfCreated() const {
    return panelHosts.getPanel2DHostComponentIfCreated();
}

void TrimeshPanelBridge::setPanelHostDelegate(TrimeshPanelHostDelegate* delegate) {
    panelHosts.setDelegate(delegate);
}

void TrimeshPanelBridge::clearPanelHostDelegate(TrimeshPanelHostDelegate* delegate) {
    panelHosts.clearDelegate(delegate);
}

void TrimeshPanelBridge::initialiseSharedGlResources() {
    panelHosts.initialiseSharedGlResources();
}

void TrimeshPanelBridge::releaseSharedGlResources() {
    panelHosts.releaseSharedGlResources();
}

void TrimeshPanelBridge::setDisplayDomain(PortDomain domain) {
    setRenderProfile(TrimeshRenderProfile::fromDomain(domain));
}

void TrimeshPanelBridge::setRenderProfile(TrimeshRenderProfile profile) {
    renderProfile = profile;
    panel3D.setRenderProfile(profile);
    panel2D.setRenderProfile(profile);
}

void TrimeshPanelBridge::renderPanel3D(Rectangle<float> bounds, float scaleFactor) {
    panelHosts.renderPanel3D(bounds, scaleFactor);
}

void TrimeshPanelBridge::renderPanel2D(Rectangle<float> bounds, float scaleFactor) {
    panelHosts.renderPanel2D(bounds, scaleFactor);
}

}
