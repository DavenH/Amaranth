#include <Definitions.h>
#include <Design/Updating/Updater.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>

#include "SpectDelegate.h"
#include "../Curve/GraphicRasterizer.h"
#include "../Util/CycleEnums.h"

SpectDelegate::SpectDelegate(SingletonRepo* repo) :
		SingletonAccessor(repo, "SpectDelegate") {
}


SpectDelegate::~SpectDelegate() {
}


void SpectDelegate::performUpdate(int updateType) {
    bool isMagnitudesMode = getSetting(MagnitudeDrawMode) == 1;

    switch (updateType) {
		case UpdateType::Repaint: break;
		case UpdateType::ReduceDetail:
		case UpdateType::RestoreDetail:
		{
			bool isPostFFT = getSetting(ViewStage) >= ViewStages::PostSpectrum;

			if(isMagnitudesMode || isPostFFT)
				getObj(SpectRasterizer).performUpdate(updateType);

			if((! isMagnitudesMode || isPostFFT))
				getObj(PhaseRasterizer).performUpdate(updateType);

			break;
		}

        case UpdateType::Update: {
			getSetting(MagnitudeDrawMode) ?
				getObj(SpectRasterizer).performUpdate(updateType) :
				getObj(PhaseRasterizer).performUpdate(updateType);
		}
	}
}
