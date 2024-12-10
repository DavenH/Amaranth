#include "MorphUpdate.h"

#include <Curve/EnvRasterizer.h>
#include <Curve/MeshRasterizer.h>
#include <Definitions.h>
#include <Design/Updating/Updater.h>
#include <Obj/MorphPosition.h>
#include <UI/Panels/Panel.h>

#include "../App/Initializer.h"
#include "../Curve/E3Rasterizer.h"
#include "../Curve/GraphicRasterizer.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/VertexPanels/Spectrum2D.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"

MorphUpdate::MorphUpdate(SingletonRepo* repo) : SingletonAccessor(repo, "MorphUpdate") {
	updateName = name;
}


MorphUpdate::~MorphUpdate() {
}


void MorphUpdate::performUpdate(int updateType) {
    switch (updateType) {
		case UpdateType::Null:
		case UpdateType::Repaint:
			break;

        case UpdateType::Update: {
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
	}
}
