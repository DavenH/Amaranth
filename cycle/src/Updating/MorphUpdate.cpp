#include "MorphUpdate.h"

#include <Definitions.h>
#include <Design/Updating/Updater.h>
#include <UI/Panels/Panel.h>

#include "../App/Initializer.h"
#include "../Wireframe/E3Rasterizer.h"
#include "../Wireframe/GraphicRasterizer.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/VertexPanels/Spectrum2D.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"

MorphUpdate::MorphUpdate(SingletonRepo* repo) : SingletonAccessor(repo, "MorphUpdate") {
    updateName = name;
}

void MorphUpdate::performUpdate(UpdateType updateType) {
    switch (updateType) {
        case Null:
        case Repaint:
            break;

        case Update: {
            getObj(Spectrum3D).updateBackground(true);
            getObj(Spectrum2D).triggerPendingScaleUpdate();

            getObj(TimeRasterizer).pullModPositionAndAdjust();
            getObj(SpectRasterizer).pullModPositionAndAdjust();
            getObj(PhaseRasterizer).pullModPositionAndAdjust();

            MorphPosition m = getObj(MorphPanel).getMorphPosition();
            m.time = 0;

//			getObj(Multisample).performUpdate();
            getObj(EnvPitchRast).setMorphPosition(m);
            getObj(EnvVolumeRast).setMorphPosition(m);
            getObj(EnvScratchRast).setMorphPosition(m);
            getObj(E3Rasterizer).setMorphPosition(m);

            // TODO make this another node -- it gets called several times per refresh
            getObj(VisualDsp).rasterizeAllEnvs(getObj(Waveform3D).getWindowWidthPixels());
            break;
        }
        default:
            break;
    }
}
