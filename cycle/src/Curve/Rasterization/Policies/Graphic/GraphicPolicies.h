#pragma once

#include <App/MeshLibrary.h>
#include <Curve/Vertex.h>
#include <Obj/MorphPosition.h>
#include <Util/CommonEnums.h>

namespace Cycle::Rasterization {
    class GraphicAxisPolicy {
    public:
        int primaryViewDimension(int currentMorphAxis) const {
            return currentMorphAxis;
        }

        bool shouldUseScratchPosition(int layerGroup, int currentMorphAxis) const {
            if (layerGroup == LayerGroups::GroupTime && currentMorphAxis != Vertex::Time) {
                return false;
            }

            return layerGroup == LayerGroups::GroupTime
                || layerGroup == LayerGroups::GroupSpect
                || layerGroup == LayerGroups::GroupPhase;
        }
    };

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
