#pragma once

#include "TrimeshPanelDataSource.h"
#include "TrimeshRenderProfile.h"

#include <Curve/Mesh/Vertex.h>
#include <UI/Panels/Panel3D.h>

namespace CycleV2 {

class TrimeshPanel3D : public Panel3D {
public:
    TrimeshPanel3D(SingletonRepo* repo, TrimeshPanelDataSource& dataSource);

    void drawViewableVerts() override {}
    bool shouldDrawGrid() override { return true; }
    int interceptLinePrimaryDimension() override { return primaryViewAxis; }
    void panelResized() override;
    void postVertsDraw() override {}
    bool usesCachedSurface() const override { return !sharedCanvasMode; }
    void setSharedCanvasMode(bool shouldUseSharedCanvas) { sharedCanvasMode = shouldUseSharedCanvas; }
    void setDisplayDomain(PortDomain domain);
    void setRenderProfile(TrimeshRenderProfile profile);
    void setPrimaryViewAxis(int axis) { primaryViewAxis = axis; }

private:
    void applyGradientForProfile();

    TrimeshPanelDataSource& dataSource;
    TrimeshRenderProfile renderProfile { TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal) };
    int primaryViewAxis { Vertex::Time };
    bool sharedCanvasMode {};
};

}
