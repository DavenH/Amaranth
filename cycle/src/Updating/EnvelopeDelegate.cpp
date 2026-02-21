#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Curve/Vertex.h>
#include <Definitions.h>
#include <Design/Updating/Updater.h>

#include "EnvelopeDelegate.h"

#include "../Curve/E3Rasterizer.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../UI/VertexPanels/Envelope3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"
#include "../Util/CycleEnums.h"

EnvelopeDelegate::EnvelopeDelegate(SingletonRepo* repo) :
        SingletonAccessor(repo, "EnvelopeDelegate") {
    updateName = name;
}

void EnvelopeDelegate::performUpdate(UpdateType performUpdateType) {
    switch (performUpdateType) {
        case Null: break;
        case Repaint: break;
        case ReduceDetail: {
            if (getObj(Envelope3D).isVisible() && getSetting(CurrentMorphAxis) != Vertex::Time) {
                getObj(E3Rasterizer).performUpdate(performUpdateType);
            }
            break;
        }

        case RestoreDetail: {
            getObj(E3Rasterizer).performUpdate(performUpdateType);
            break;
        }

        case Update: {
            if (getObj(Envelope3D).isVisible() && getSetting(CurrentMorphAxis) != Vertex::Time) {
                getObj(E3Rasterizer).performUpdate(performUpdateType);
            } else {
                getObj(EnvelopeInter2D).getRasterizer()->performUpdate(performUpdateType);
            }

            getObj(VisualDsp).rasterizeEnv(getSetting(CurrentEnvGroup),
                                           getObj(Waveform3D).getWindowWidthPixels());

            break;
        }

        default:
            break;
    }
}
