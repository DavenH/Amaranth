#pragma once

#include <App/MeshLibrary.h>
#include <Curve/Vertex.h>

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
}
