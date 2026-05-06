#pragma once

#include "../Policies/GraphicAxisPolicy.h"
#include "../Policies/GraphicMorphPositionPolicy.h"

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
