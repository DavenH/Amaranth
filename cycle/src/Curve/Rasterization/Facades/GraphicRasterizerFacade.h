#pragma once

#include "../Policies/Graphic/GraphicAxisPolicy.h"
#include "../Policies/Graphic/GraphicMorphPositionPolicy.h"

namespace Cycle::Rasterization {
    class GraphicRasterizerFacade {
    public:
        int primaryViewDimension(int currentMorphAxis) const {
            return axisPolicy.primaryViewDimension(currentMorphAxis);
        }

        MorphPosition resolveMorphPosition(const GraphicMorphPositionPolicy::Context& context) const {
            return morphPositionPolicy.resolve(context);
        }

    private:
        GraphicAxisPolicy axisPolicy;
        GraphicMorphPositionPolicy morphPositionPolicy;
    };
}
