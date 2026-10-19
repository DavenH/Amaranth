#pragma once

#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"

#include <Curve/Mesh/Vertex.h>
#include <UI/Panels/Panel3D.h>

#include <functional>
#include <vector>

namespace CycleV2 {

class TrimeshPanel3D : public Panel3D {
public:
    TrimeshPanel3D(SingletonRepo* repo, Panel3D::DataRetriever& dataSource);

    void drawViewableVerts() override {}
    bool shouldDrawGrid() override { return true; }
    bool willAdjustSurfaceColumns() override { return pitchSpansColumns; }
    bool getScalarSurfaceMaterial(ScalarSurfaceMaterial& material) const override;
    bool shouldStepSurfaceRows() const override {
        return renderProfile.getDomain() == PortDomain::SpectralMagnitudeSignal;
    }
    Buffer<Float32> getLineGuideTable(int channel) override;
    int interceptLinePrimaryDimension() override { return primaryViewAxis; }
    void drawBackground(bool fillBackground = true) override;
    void panelResized() override;
    void zoomUpdated(int updateSource) override;
    void doZoomExtra(bool commandDown) override;
    void updateBackground(bool onlyVerticalBackground = false) override;
    void postVertsDraw() override {}
    bool usesCachedSurface() const override { return !sharedCanvasMode; }
    void setSharedCanvasMode(bool shouldUseSharedCanvas) { sharedCanvasMode = shouldUseSharedCanvas; }
    void setDisplayDomain(PortDomain domain);
    void setRenderProfile(TrimeshRenderProfile profile);
    void configureViewport(Range<float> frequencyLimits);
    void setZoomChangedCallback(std::function<void(bool)> callback);
    void setPrimaryViewAxis(int axis) { primaryViewAxis = axis; }
    void setPitchSpansColumns(bool shouldSpan);
    void setPreviewMidiNote(int midiNote);
    void setLineGuideTables(std::vector<std::vector<float>> tables);

private:
    void applyGradientForProfile();
    void drawSpectralHarmonics();
    bool drawHarmonicLine(
            int harmonic,
            Buffer<float> x,
            Buffer<float> y,
            const std::vector<Buffer<float>>& pitchRamps);

    TrimeshRenderProfile renderProfile { TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal) };
    std::vector<std::vector<float>> lineGuideTables;
    std::function<void(bool)> zoomChangedCallback;
    int primaryViewAxis { Vertex::Time };
    int previewMidiNote { 48 };
    int appliedMaterialStyleSignature { -2 };
    bool pitchSpansColumns {};
    bool sharedCanvasMode {};
};

}
