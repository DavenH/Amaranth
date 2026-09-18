#pragma once

#include "Nodes/Trimesh/Panel/TrimeshPanelDataSource.h"
#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"

#include <Curve/Mesh/Vertex.h>
#include <UI/Panels/Panel3D.h>

#include <vector>

namespace CycleV2 {

class TrimeshPanel3D : public Panel3D {
public:
    TrimeshPanel3D(SingletonRepo* repo, TrimeshPanelDataSource& dataSource);

    void drawViewableVerts() override {}
    bool shouldDrawGrid() override { return true; }
    bool willAdjustSurfaceColumns() override { return pitchSpansColumns; }
    int interceptLinePrimaryDimension() override { return primaryViewAxis; }
    void drawBackground(bool fillBackground = true) override;
    void panelResized() override;
    void updateBackground(bool onlyVerticalBackground = false) override;
    void postVertsDraw() override {}
    bool usesCachedSurface() const override { return !sharedCanvasMode; }
    void setSharedCanvasMode(bool shouldUseSharedCanvas) { sharedCanvasMode = shouldUseSharedCanvas; }
    void setDisplayDomain(PortDomain domain);
    void setRenderProfile(TrimeshRenderProfile profile);
    void setPrimaryViewAxis(int axis) { primaryViewAxis = axis; }
    void setPitchSpansColumns(bool shouldSpan);
    void setPreviewMidiNote(int midiNote);

private:
    void applyGradientForProfile();
    void drawSpectralHarmonics();
    bool drawHarmonicLine(
            int harmonic,
            Buffer<float> x,
            Buffer<float> y,
            const std::vector<Buffer<float>>& pitchRamps);

    TrimeshPanelDataSource& dataSource;
    TrimeshRenderProfile renderProfile { TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal) };
    int primaryViewAxis { Vertex::Time };
    int previewMidiNote { 48 };
    bool pitchSpansColumns {};
    bool sharedCanvasMode {};
};

}
