#pragma once

#include "TrimeshPanel2D.h"
#include "TrimeshPanel3D.h"
#include "TrimeshPanelEnvironment.h"
#include "TrimeshPanelHostDelegate.h"
#include "TrimeshPanelHosts.h"
#include "TrimeshPanelRasterizer.h"
#include "TrimeshInteractor2D.h"
#include "TrimeshInteractor3D.h"
#include "TrimeshInvalidation.h"

#include <cstdint>
#include <functional>

struct Intercept;

namespace CycleV2 {

class TrimeshPanelBridge {
public:
    TrimeshPanelBridge();
    ~TrimeshPanelBridge();

    void syncFromNode(
            const Node& node,
            int rows,
            int columns);

    TrimeshPanel3D& getPanel3D() { return panel3D; }
    TrimeshPanel2D& getPanel2D() { return panel2D; }
    TrimeshPanelDataSource& getDataSource() { return dataSource; }
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
    void setMeshEditedCallback(std::function<void()> callback);
    void initialiseSharedGlResources();
    void releaseSharedGlResources();
    void setDisplayDomain(PortDomain domain);
    void setRenderProfile(TrimeshRenderProfile profile);
    void renderPanel3D(juce::Rectangle<float> bounds, float scaleFactor);
    void renderPanel2D(juce::Rectangle<float> bounds, float scaleFactor);
    int selectedVertexIndexForPanel();

private:
    void refreshAfterMeshEdit(TrimeshMeshEditEvent event);
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
    std::function<void()> meshEditedCallback;
    uint64_t lastSyncedRevision { UINT64_MAX };
    PortDomain lastRenderDomain { PortDomain::ControlSignal };
    int lastRows {};
    int lastColumns {};
};

}
