#pragma once

#include "TrimeshRenderProfile.h"

#include <UI/Panels/Panel2D.h>

namespace CycleV2 {

class TrimeshPanel2D : public Panel2D {
public:
    explicit TrimeshPanel2D(SingletonRepo* repo);

    void drawBackground(bool fillBackground = true) override;
    void drawViewableVerts() override {}
    void panelResized() override;
    void setDisplayDomain(PortDomain domain);

private:
    void applyRenderProfile();
    void drawWaveformBackground(bool fillBackground);
    void drawSpectrumBackground(bool fillBackground);

    TrimeshRenderProfile renderProfile { TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal) };
};

}
