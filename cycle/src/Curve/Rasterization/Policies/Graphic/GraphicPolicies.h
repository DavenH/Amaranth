#pragma once

#include <App/MeshLibrary.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Rasterization/ScratchPositionPolicy.h>
#include <Obj/MorphPosition.h>
#include <Util/CommonEnums.h>

namespace Cycle::Rasterization {
    class GraphicAxisPolicy {
    public:
        int primaryViewDimension(int currentMorphAxis) const {
            return currentMorphAxis;
        }

        bool shouldUseScratchPosition(int layerGroup, int currentMorphAxis) const {
            return ::Rasterization::ScratchPositionPolicy::shouldApply(
                    scratchDomain(layerGroup), currentMorphAxis);
        }

        ::Rasterization::ScratchSourceDomain scratchDomain(int layerGroup) const {
            if (layerGroup == LayerGroups::GroupTime) {
                return ::Rasterization::ScratchSourceDomain::Time;
            }
            if (layerGroup == LayerGroups::GroupSpect
                    || layerGroup == LayerGroups::GroupPhase) {
                return ::Rasterization::ScratchSourceDomain::Spectral;
            }
            return ::Rasterization::ScratchSourceDomain::Unsupported;
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

            morph = ::Rasterization::ScratchPositionPolicy::resolve(
                    morph,
                    axisPolicy.scratchDomain(context.layerGroup),
                    context.currentMorphAxis,
                    context.scratchPosition);

            return morph;
        }

    private:
        GraphicAxisPolicy axisPolicy;
    };
}
