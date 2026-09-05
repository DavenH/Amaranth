#pragma once

#include "Nodes/Trimesh/Panel/TrimeshPanel2D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanel3D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelEnvironment.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelHostDelegate.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelHosts.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelRasterizer.h"
#include "Nodes/Trimesh/Panel/TrimeshInteractor2D.h"
#include "Nodes/Trimesh/Panel/TrimeshInteractor3D.h"
#include "Nodes/Trimesh/Model/TrimeshInvalidation.h"
#include "Nodes/Trimesh/Dsp/TrimeshGuidePreparation.h"

#include <cstdint>
#include <functional>

struct Intercept;

namespace CycleV2 {

class TrimeshPanelBridge : private juce::Timer {
public:
    TrimeshPanelBridge();
    ~TrimeshPanelBridge();

    void syncFromNode(
            const Node& node,
            int rows,
            int columns);
    void applyPreparedGuides(PreparedTrimeshGuides guides);

    TrimeshPanel3D& getPanel3D() { return panel3D; }
    TrimeshPanel2D& getPanel2D() { return panel2D; }
    const TrimeshPanel2D& getPanel2D() const { return panel2D; }
    TrimeshPanelDataSource& getDataSource() { return dataSource; }
    const TrimeshPanelDataSource& getDataSource() const { return dataSource; }
    const TrimeshRenderData& getRenderData() const { return dataSource.getRenderData(); }
    Interactor2D& getInteractor2D() { return interactor2D; }
    const Interactor2D& getInteractor2D() const { return interactor2D; }
    Interactor3D& getInteractor3D() { return interactor3D; }
    TrimeshNodeModel& getModel() { return model; }
    bool rasterizerWrapsVertices() { return panelRasterizer.wrapsVertices(); }
    Component* getPanel3DHostComponent();
    Component* getPanel3DHostComponentIfCreated() const;
    Component* getPanel2DHostComponent();
    Component* getPanel2DHostComponentIfCreated() const;
    void setPanelHostDelegate(TrimeshPanelHostDelegate* delegate);
    void clearPanelHostDelegate(TrimeshPanelHostDelegate* delegate);
    void setMeshEditedCallback(std::function<void(TrimeshMeshEditEvent)> callback);
    void initialiseSharedGlResources();
    void releaseSharedGlResources();
    void setDisplayDomain(PortDomain domain);
    void setRenderProfile(TrimeshRenderProfile profile);
    void setPreviewMidiNote(int midiNote);
    void setPreviewKeyScaleAxis(int axis);
    void renderPanel3D(juce::Rectangle<float> bounds, float scaleFactor);
    void renderPanel2D(juce::Rectangle<float> bounds, float scaleFactor);
    int selectedVertexIndexForPanel();

private:
    void refreshAfterMeshEdit(TrimeshMeshEditEvent event);
    void flushPendingMeshEdit(bool gestureComplete);
    void timerCallback() override;
    void clearInteractionPointers();
    void updateGuideCurveSeeds();
    void syncPrimaryAxisContext();
    void updateRasterizer(bool refresh2DPanel, bool refresh3DGeometry);

    TrimeshPanelEnvironment environment;
    TrimeshNodeModel model;
    TrimeshInvalidation invalidation;
    TrimeshPanelDataSource dataSource;
    TrimeshPanelRasterizer panelRasterizer;
    TrimeshInteractor2D interactor2D;
    TrimeshInteractor3D interactor3D;
    TrimeshPanel2D panel2D;
    TrimeshPanel3D panel3D;
    TrimeshPanelHosts panelHosts;
    TrimeshRenderProfile renderProfile { TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal) };
    std::function<void(TrimeshMeshEditEvent)> meshEditedCallback;
    std::shared_ptr<GuideCurveSnapshotProvider> guideCurveProvider;
    uint64_t lastSyncedRevision { UINT64_MAX };
    PortDomain lastRenderDomain { PortDomain::ControlSignal };
    RenderScalePolicy lastRenderScalePolicy { RenderScalePolicy::Bipolar };
    int lastRows {};
    int lastColumns {};
    int previewMidiNote { 48 };
    int lastPreviewMidiNote { -1 };
    int previewKeyScaleAxis { -1 };
    int lastPreviewKeyScaleAxis { -2 };
    bool pendingMeshEdit {};
    bool pendingMeshEditSourceIs3D {};
};

}
