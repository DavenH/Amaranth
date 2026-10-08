#pragma once

#include <UI/Panels/Panel2D.h>

#include <vector>

#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"

namespace CycleV2 {

class TrimeshPanelDataSource;

class TrimeshPanel2D : public Panel2D {
public:
    TrimeshPanel2D(SingletonRepo* repo, TrimeshPanelDataSource& source);

    void preDraw() override;
    void drawBackground(bool fillBackground = true) override;
    void drawViewableVerts() override {}
    void panelResized() override;
    void setDisplayDomain(PortDomain domain);
    void setRenderProfile(TrimeshRenderProfile profile);
    void setPreviewMidiNote(int midiNote);
    void setPrimaryMorphPosition(float position);

private:
    void applyRenderProfile();
    void drawSpectralPartials();
    void drawWaveformBackground(bool fillBackground);
    void drawSpectrumMagnitudeBackground(bool fillBackground);
    void drawSpectrumPhaseBackground(bool fillBackground);

    TrimeshPanelDataSource& dataSource;
    TrimeshRenderProfile renderProfile { TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal) };
    std::vector<float> partialValues;
    std::vector<ColorPos> partialContour;
    float primaryMorphPosition {};
    int previewMidiNote { 48 };
};

}
