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

namespace {
const char* envGroupName(int envGroup) {
    switch (envGroup) {
        case LayerGroups::GroupVolume: return "volume";
        case LayerGroups::GroupPitch: return "pitch";
        case LayerGroups::GroupScratch: return "scratch";
        case LayerGroups::GroupWavePitch: return "wavePitch";
        default: return "unknown";
    }
}
}

EnvelopeDelegate::EnvelopeDelegate(SingletonRepo* repo) :
        SingletonAccessor(repo, "EnvelopeDelegate") {
    updateName = name;
}

void EnvelopeDelegate::performUpdate(UpdateType performUpdateType) {
    switch (performUpdateType) {
        case Null: break;
        case Repaint: break;
        case ReduceDetail: {
            if(getObj(Envelope3D).isVisible() && getSetting(CurrentMorphAxis) != Vertex::Time) {
                getObj(E3Rasterizer).performUpdate(performUpdateType);
            }
            break;
        }

        case RestoreDetail: {
            int envGroup = getSetting(CurrentEnvGroup);
            bool usesEnvelope3D = getObj(Envelope3D).isVisible() && getSetting(CurrentMorphAxis) != Vertex::Time;
            DBG(String::formatted("EnvelopeDelegate::RestoreDetail currentEnv=%s(%d) usesEnvelope3D=%d",
                                  envGroupName(envGroup),
                                  envGroup,
                                  usesEnvelope3D ? 1 : 0));
            getObj(E3Rasterizer).performUpdate(performUpdateType);
            break;
        }

        case Update: {
            int envGroup = getSetting(CurrentEnvGroup);
            bool usesEnvelope3D = getObj(Envelope3D).isVisible() && getSetting(CurrentMorphAxis) != Vertex::Time;

            DBG(String::formatted("EnvelopeDelegate::Update currentEnv=%s(%d) usesEnvelope3D=%d",
                                  envGroupName(envGroup),
                                  envGroup,
                                  usesEnvelope3D ? 1 : 0));

            if(usesEnvelope3D) {
                DBG("EnvelopeDelegate::Update refreshing Envelope3D rasterizer");
                getObj(E3Rasterizer).performUpdate(performUpdateType);
            } else {
                auto* currentRasterizer = getObj(EnvelopeInter2D).getRasterizer();
                DBG(String::formatted("EnvelopeDelegate::Update refreshing current EnvelopeInter2D rasterizer=%p",
                                      currentRasterizer));
                getObj(EnvelopeInter2D).getRasterizer()->performUpdate(performUpdateType);
            }

            DBG(String::formatted("EnvelopeDelegate::Update requesting VisualDsp::rasterizeEnv currentEnv=%s(%d)",
                                  envGroupName(envGroup),
                                  envGroup));
            getObj(VisualDsp).rasterizeEnv(envGroup,
                                           getObj(Waveform3D).getWindowWidthPixels());

            break;
        }

        default:
            break;
    }
}
