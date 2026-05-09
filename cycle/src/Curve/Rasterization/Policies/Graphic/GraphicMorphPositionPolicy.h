#pragma once

#include <Obj/MorphPosition.h>
#include <Util/CommonEnums.h>

#include "GraphicAxisPolicy.h"

namespace Cycle::Rasterization {
    class GraphicMorphPositionPolicy {
    public:
        struct Context {
            MorphPosition panelMorph;
            int layerGroup {};
            int currentMorphAxis {};
            int scratchChannel { CommonEnums::Null };
            float scratchPosition {};
        };

        MorphPosition resolve(const Context& context) const {
            MorphPosition morph = context.panelMorph;

            if (context.scratchChannel == CommonEnums::Null) {
                return morph;
            }

            if (axisPolicy.shouldUseScratchPosition(context.layerGroup, context.currentMorphAxis)) {
                morph.time = context.scratchPosition;
            }

            return morph;
        }

    private:
        GraphicAxisPolicy axisPolicy;
    };
}
