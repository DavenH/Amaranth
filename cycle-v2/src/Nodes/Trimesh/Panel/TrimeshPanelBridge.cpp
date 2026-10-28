#include "Nodes/Trimesh/Panel/TrimeshPanelBridge.h"
#include "Graph/MeshDimensionality.h"

#include "Graph/NodeParameterMap.h"
#include "Nodes/Trimesh/Rendering/TrimeshGuideRailRenderService.h"

#include <App/AppConstants.h>
#include <Curve/Mesh/Vertex.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegionMapping.h>

namespace CycleV2 {

namespace {

uint64_t panelRevisionFor(const TrimeshNodeModel& model) {
    const TrimeshDerivedRevisions& revisions = model.getDerivedRevisions();
    return jmax(
            revisions.sliceRasterization,
            revisions.interceptsRails,
            revisions.columns3D);
}

int panelMidiNoteFor(
        const MorphPosition& morph,
        int keyScaleAxis,
        int previewMidiNote) {
    if (keyScaleAxis != Vertex::Time && keyScaleAxis != Vertex::Red && keyScaleAxis != Vertex::Blue) {
        return previewMidiNote;
    }

    return Arithmetic::getGraphicNoteForValue(
            morph[keyScaleAxis].getTargetValue(),
            Range<int>(Constants::LowestMidiNote, Constants::HighestMidiNote));
}

}

TrimeshPanelBridge::TrimeshPanelBridge() :
        interactor2D    (&environment.getRepo(), "CycleV2TrimeshInteractor2D",
                         Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Time, Vertex::Red, Vertex::Blue))
    ,   interactor3D    (&environment.getRepo(), "CycleV2TrimeshInteractor3D")
    ,   panel2D         (&environment.getRepo(), dataSource)
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
    stopTimer();
    interactor2D.stopTimer();
    interactor3D.stopTimer();
    panel2D.setInteractor(nullptr);
    panel3D.setInteractor(nullptr);
    interactor2D.setRasterizer(nullptr);
    interactor3D.setRasterizer(nullptr);
    releaseSharedGlResources();
}

bool TrimeshPanelBridge::applyPreparedGuides(PreparedTrimeshGuides guides) {
    if (guides.mesh == nullptr || guides.provider == nullptr) {
        return false;
    }
    if (meshEditGestureActive) {
        return false;
    }

    guideCurveProvider = std::move(guides.provider);
    environment.getRepo().setGuideCurveProvider(guideCurveProvider.get());
    panelRasterizer.getRasterizer().setGuideCurveProvider(guideCurveProvider.get());
    updateGuideCurveSeeds();
    if (model.applyPreparedGuides(*guides.mesh, guideCurveProvider)) {
        clearInteractionPointers();
    }
    dataSource.rebuild(
            model,
            lastRows,
            lastColumns,
            renderProfile,
            panelMidiNote,
            previewKeyScaleAxis);
    updateRasterizer(true, true);
    lastSyncedRevision = panelRevisionFor(model);
    return true;
}

void TrimeshPanelBridge::syncFromNode(
        const Node& node,
        int rows,
        int columns) {
    const NodeParameterMap parameters(node);
    const auto dimensions = MeshDimensionality::dimensions(node);
    const bool yellowLinked = !dimensions.active(Vertex::Time) || parameters.boolValue("link.yellow", true);
    const bool redLinked = !dimensions.active(Vertex::Red) || parameters.boolValue("link.red", true);
    const bool blueLinked = !dimensions.active(Vertex::Blue) || parameters.boolValue("link.blue", true);
    const bool linksChanged = lastYellowLink < 0
            || lastRedLink < 0
            || lastBlueLink < 0
            || yellowLinked != (lastYellowLink == 1)
            || redLinked != (lastRedLink == 1)
            || blueLinked != (lastBlueLink == 1);
    environment.setAxisLinks(yellowLinked, redLinked, blueLinked);
    const bool keyScaleLinked = previewKeyScaleAxis == Vertex::Time ? yellowLinked
            : previewKeyScaleAxis == Vertex::Red ? redLinked : blueLinked;
    panel3D.setKeyScaleAxis(dimensions.active(previewKeyScaleAxis) && keyScaleLinked
            ? previewKeyScaleAxis : -1);
    const bool spectral = renderProfile.getDomain() == PortDomain::SpectralMagnitudeSignal
            || renderProfile.getDomain() == PortDomain::SpectralPhaseSignal;
    const MorphPosition effectiveMorph = resolveMorphPosition(node);
    panelMidiNote = panelMidiNoteFor(effectiveMorph, previewKeyScaleAxis, previewMidiNote);
    panel2D.setPreviewMidiNote(panelMidiNote);
    panel3D.setPreviewMidiNote(panelMidiNote);

    const uint64_t previousPanelRevision = panelRevisionFor(model);
    const int previousPrimaryAxis = model.getPrimaryViewAxis();
    const MorphPosition previousMorph = model.getMorphPosition();

    const bool meshReplaced = !meshEditGestureActive
            && model.syncFromNode(
                    node,
                    morphEditGestureActive
                            ? TrimeshSelectionSyncPolicy::PreserveCurrent
                            : TrimeshSelectionSyncPolicy::SynchronizeFromNode,
                    effectiveMorph);
    if (meshReplaced) {
        stopTimer();
        pendingMeshEdit = false;
        clearInteractionPointers();
    }
    if (linksChanged && !meshReplaced) {
        interactor2D.setMovingVertsFromSelected();
        interactor3D.setMovingVertsFromSelected();
        panel2D.requestRepaint();
        panel3D.requestRepaint();
    }
    lastYellowLink = yellowLinked ? 1 : 0;
    lastRedLink = redLinked ? 1 : 0;
    lastBlueLink = blueLinked ? 1 : 0;
    environment.setMorphPosition(model.getMorphPosition(), model.getPrimaryViewAxis());
    panel2D.setPrimaryMorphPosition(
            model.getMorphPosition()[model.getPrimaryViewAxis()].getTargetValue());
    syncPrimaryAxisContext();
    const bool pitchSpansColumns = spectral
            && model.getPrimaryViewAxis() == previewKeyScaleAxis;
    panel3D.setPitchSpansColumns(pitchSpansColumns);
    if (spectral) {
        rows = LogRegionMapping(
                pitchSpansColumns
                        ? Constants::LowestMidiNote
                        : panelMidiNote).regionSize();
    }

    const uint64_t nextPanelRevision = panelRevisionFor(model);
    const bool panelDataChanged = previousPanelRevision != nextPanelRevision;
    const bool primaryAxisChanged = previousPrimaryAxis != model.getPrimaryViewAxis();
    const MorphPosition& nextMorph = model.getMorphPosition();
    const bool yellowChanged = previousMorph.time.getTargetValue() != nextMorph.time.getTargetValue();
    const bool redChanged = previousMorph.red.getTargetValue() != nextMorph.red.getTargetValue();
    const bool blueChanged = previousMorph.blue.getTargetValue() != nextMorph.blue.getTargetValue();
    const bool morphChanged = yellowChanged || redChanged || blueChanged;
    const bool renderDomainChanged = lastRenderDomain != renderProfile.getDomain();
    const bool renderScaleChanged = lastRenderScalePolicy != renderProfile.getScalePolicy();
    const bool gridShapeChanged = lastRows != rows || lastColumns != columns;
    const bool previewPitchChanged = lastPanelMidiNote != panelMidiNote;
    const bool keyScaleAxisChanged = lastPreviewKeyScaleAxis != previewKeyScaleAxis;

    if (!panelDataChanged
            && !morphChanged
            && !primaryAxisChanged
            && !renderDomainChanged
            && !renderScaleChanged
            && !previewPitchChanged
            && !keyScaleAxisChanged
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
    change.renderDomainChanged = renderDomainChanged || renderScaleChanged;

    const TrimeshInvalidationResult invalidated = invalidation.invalidate(change);
    if (invalidated.rebuildSurfaceData) {
        dataSource.rebuild(
                model,
                rows,
                columns,
                renderProfile,
                panelMidiNote,
                previewKeyScaleAxis);
    } else if (invalidated.rebuildSliceData) {
        dataSource.rebuildSlice(
                model,
                rows,
                renderProfile,
                panelMidiNote);
    }
    updateRasterizer(invalidated.refresh2DPanel, invalidated.refresh3DGeometry);
    lastSyncedRevision = nextPanelRevision;
    lastRenderDomain = renderProfile.getDomain();
    lastRenderScalePolicy = renderProfile.getScalePolicy();
    lastRows = rows;
    lastColumns = columns;
    lastPanelMidiNote = panelMidiNote;
    lastPreviewKeyScaleAxis = previewKeyScaleAxis;
}

void TrimeshPanelBridge::refreshAfterMeshEdit(TrimeshMeshEditEvent event) {
    if (event.selectionOnly) {
        vector<Vertex*>& selected = event.sourceIs3D
                ? interactor3D.getSelected()
                : interactor2D.getSelected();
        const bool selectionChanged = model.selectVertex(
                selected.empty() ? nullptr : selected.front());
        pendingSelectionChanged |= selectionChanged;

        if (selectionChanged) {
            panel2D.requestRepaint();
            panel3D.requestRepaint();
        }
        if (!event.gestureComplete) {
            meshEditGestureActive = true;
            return;
        }
        if (pendingSelectionChanged && meshEditedCallback != nullptr) {
            meshEditedCallback(event);
        }
        pendingSelectionChanged = false;
        meshEditGestureActive = false;
        return;
    }

    if (!event.gestureComplete) {
        meshEditGestureActive = true;
    }
    model.markMeshEdited();
    syncPrimaryAxisContext();

    const TrimeshInvalidationResult invalidated = invalidation.invalidate({
            TrimeshChangeKind::MeshEdit,
            false,
            false,
            false,
            event.sourceIs3D,
            model.getPrimaryViewAxis()
    });
    updateRasterizer(invalidated.refresh2DPanel, invalidated.refresh3DGeometry);
    lastSyncedRevision = panelRevisionFor(model);

    pendingMeshEdit = true;
    pendingMeshEditSourceIs3D = event.sourceIs3D;
    if (event.gestureComplete) {
        stopTimer();
        flushPendingMeshEdit(true);
    } else if (!isTimerRunning()) {
        startTimerHz(30);
    }
}

void TrimeshPanelBridge::setPreviewMorphPosition(std::optional<MorphPosition> position) {
    editorMorph.setPreview(std::move(position));
}

MorphPosition TrimeshPanelBridge::resolveMorphPosition(const Node& node) {
    return editorMorph.resolve(MeshDimensionality::position(node), morphEditGestureActive);
}

void TrimeshPanelBridge::setEditorMorphPosition(int axis, float value) {
    editorMorph.setValue(axis, value);
}

void TrimeshPanelBridge::resetDocumentPresentation() {
    editorMorph.reset();
    morphEditGestureActive = false;
}

void TrimeshPanelBridge::flushPendingMeshEdit(bool gestureComplete) {
    if (!pendingMeshEdit) {
        return;
    }

    pendingMeshEdit = false;

    if (gestureComplete) {
        dataSource.rebuild(
                model,
                lastRows,
                lastColumns,
                renderProfile,
                panelMidiNote,
                previewKeyScaleAxis);
    }

    if (meshEditedCallback != nullptr) {
        meshEditedCallback({ pendingMeshEditSourceIs3D, gestureComplete });
    }
    if (gestureComplete) {
        pendingSelectionChanged = false;
        meshEditGestureActive = false;
    }
}

void TrimeshPanelBridge::timerCallback() {
    flushPendingMeshEdit(false);
}

void TrimeshPanelBridge::setMeshEditedCallback(
        std::function<void(TrimeshMeshEditEvent)> callback) {
    meshEditedCallback = std::move(callback);
}

void TrimeshPanelBridge::clearInteractionPointers() {
    interactor2D.clearSelectedAndCurrent();
    interactor3D.clearSelectedAndCurrent();
}

int TrimeshPanelBridge::selectedVertexIndexForPanel() const {
    return model.getSelectedVertexIndex();
}

void TrimeshPanelBridge::syncPrimaryAxisContext() {
    interactor3D.setPrimaryViewAxis(model.getPrimaryViewAxis());
    panel3D.setPrimaryViewAxis(model.getPrimaryViewAxis());
    panel3D.setSlicePosition(model.getMorphPosition()[model.getPrimaryViewAxis()]);
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
    const bool domainChanged = renderProfile.getDomain() != profile.getDomain();
    const bool wasMagnitude = renderProfile.getDomain()
            == PortDomain::SpectralMagnitudeSignal;
    const bool isMagnitude = profile.getDomain()
            == PortDomain::SpectralMagnitudeSignal;
    const bool enteringMagnitude = isMagnitude && !wasMagnitude;
    renderProfile = profile;
    const Range<float> frequencyLimits = profile.getSliceStyle().isSpectral()
            ? Range<float>(-0.5f, 1.5f)
            : Range<float>(0.f, 1.f);
    interactor2D.vertexLimits[Vertex::Phase] = frequencyLimits;
    interactor3D.vertexLimits[Vertex::Phase] = frequencyLimits;
    if (domainChanged) {
        updateGuideCurveSeeds();
    }
    panel3D.setRenderProfile(profile);
    panel2D.setRenderProfile(profile);
    const float defaultStart = isMagnitude
            ? LogRegionMapping(panelMidiNote).displayUnitForSourceUnit(0.f)
            : 0.f;
    panelHosts.configureFrequencyViewport(
            frequencyLimits, defaultStart, enteringMagnitude);
}

void TrimeshPanelBridge::setPreviewMidiNote(int midiNote) {
    previewMidiNote = jlimit(
            (int) Constants::LowestMidiNote,
            (int) Constants::HighestMidiNote,
            midiNote);
}

void TrimeshPanelBridge::setPreviewKeyScaleAxis(int axis) {
    previewKeyScaleAxis = axis;
}

void TrimeshPanelBridge::updateGuideCurveSeeds() {
    if (guideCurveProvider == nullptr) {
        return;
    }

    const uint32_t stableSeed = GuideCurveSnapshotProvider::visualizationSeed(
            renderProfile.getDomain());
    panelRasterizer.getRasterizer().updateOffsetSeeds(
            guideCurveProvider->size(),
            GuideCurveProvider::tableSize,
            Rasterization::GuideCurveSeed::visualization(stableSeed));
    panelRasterizer.getRasterizer().setNoiseSeed(
            (int) (stableSeed % GuideCurveProvider::tableSize));

    panel3D.setLineGuideTables(TrimeshGuideRailRenderService::prepareTables(
            *guideCurveProvider,
            stableSeed));
}

void TrimeshPanelBridge::renderPanel3D(Rectangle<float> bounds, float scaleFactor) {
    panelHosts.renderPanel3D(bounds, scaleFactor);
}

bool TrimeshPanelBridge::renderPreview2D(Rectangle<float> bounds, float scaleFactor) {
    return panelHosts.renderPreview2D(bounds, scaleFactor, model.getDerivedRevisions().aggregate);
}

bool TrimeshPanelBridge::paintPreview2D(Graphics& graphics, Rectangle<float> bounds) const {
    return panelHosts.paintPreview2D(graphics, bounds);
}

void TrimeshPanelBridge::renderPanel2D(Rectangle<float> bounds, float scaleFactor) {
    panelHosts.renderPanel2D(bounds, scaleFactor);
}

}
