#include "JuceHeader.h"
#include <Definitions.h>
#include <Design/Updating/Updater.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>

#include "SpectDelegate.h"
#include "../Wireframe/GraphicRasterizer.h"
#include "../Util/CycleEnums.h"

SpectDelegate::SpectDelegate(SingletonRepo* repo) :
        SingletonAccessor(repo, "SpectDelegate") {
}

void SpectDelegate::performUpdate(UpdateType updateType) {
    bool isMagnitudesMode = getSetting(MagnitudeDrawMode) == 1;

    switch (updateType) {
        case Repaint: break;
        case ReduceDetail:
        case RestoreDetail: {
            bool isPostFFT = getSetting(ViewStage) >= ViewStages::PostSpectrum;

            if(isMagnitudesMode || isPostFFT) {
                getObj(SpectRasterizer).performUpdate(updateType);
            }

            if((! isMagnitudesMode || isPostFFT)) {
                getObj(PhaseRasterizer).performUpdate(updateType);
            }

            break;
        }

        case Update: {
            getSetting(MagnitudeDrawMode) ?
                getObj(SpectRasterizer).performUpdate(updateType) :
                getObj(PhaseRasterizer).performUpdate(updateType);
        }

        default:
            break;
    }
}
