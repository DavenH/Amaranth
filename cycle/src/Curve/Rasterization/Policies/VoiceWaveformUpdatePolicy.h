#pragma once

#include <utility>

#include <Curve/Rasterization/RasterizerRuntime.h>

#include "../Facades/VoiceRasterizerFacade.h"

namespace Cycle::Rasterization {
    class VoiceWaveformUpdatePolicy {
    public:
        template<typename CleanUp, typename Prepare, typename Bake>
        bool update(
                ::Rasterization::RasterizerRuntime runtime,
                CleanUp&& cleanUp,
                Prepare&& prepare,
                Bake&& bake) const {
            if (!runtime.hasAtLeastIntercepts(2)) {
                cleanUp();
                return false;
            }

            facade.applyCurveResolution(runtime);
            prepare();
            bake();

            return true;
        }

    private:
        VoiceRasterizerFacade facade;
    };
}
