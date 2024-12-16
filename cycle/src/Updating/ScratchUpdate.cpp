#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Definitions.h>
#include <Design/Updating/Updater.h>

#include "ScratchUpdate.h"

#include "../App/Initializer.h"
#include "../Audio/SynthAudioSource.h"
#include "../Curve/GraphicRasterizer.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"

ScratchUpdate::ScratchUpdate(SingletonRepo* repo) : SingletonAccessor(repo, "ScratchUpdate") {
    updateName = name;
}

void ScratchUpdate::performUpdate(int updateType) {
    switch (updateType) {
		case Null:
		case Repaint:
			break;

		case Update: {
			getObj(EnvScratchRast).performUpdate(updateType);
			getObj(VisualDsp).rasterizeEnv(LayerGroups::GroupScratch,
			                               getObj(Waveform3D).getWindowWidthPixels());

			getObj(TimeRasterizer).pullModPositionAndAdjust();
			getObj(SpectRasterizer).pullModPositionAndAdjust();
			getObj(PhaseRasterizer).pullModPositionAndAdjust();

			getObj(MorphPanel).updateCube();
			getObj(EnvelopeInter2D).performUpdate(updateType);
			getObj(SynthAudioSource).setPendingGlobalRaster();
		}

		case RestoreDetail:
		case ReduceDetail:
			break;
    	default: break;
	}
}
