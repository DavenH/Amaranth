#pragma once

#include <App/MeshLibrary.h>

#include "CycleLayerGroups.h"

namespace CycleMeshLibraryConfig {
    inline MeshLibrary::GroupBindings createGroupBindings() {
        MeshLibrary::GroupBindings bindings;

        bindings.scratch = LayerGroups::GroupScratch;
        bindings.guideCurve = LayerGroups::GroupGuideCurve;
        bindings.wavePitch = LayerGroups::GroupWavePitch;
        bindings.scratchSourceGroups.add(LayerGroups::GroupTime);
        bindings.scratchSourceGroups.add(LayerGroups::GroupSpect);
        bindings.scratchSourceGroups.add(LayerGroups::GroupPhase);
        bindings.guideCurveSourceGroups.add(LayerGroups::GroupTime);
        bindings.guideCurveSourceGroups.add(LayerGroups::GroupSpect);
        bindings.guideCurveSourceGroups.add(LayerGroups::GroupPhase);
        bindings.guideCurveSourceGroups.add(LayerGroups::GroupVolume);
        bindings.guideCurveSourceGroups.add(LayerGroups::GroupPitch);
        bindings.guideCurveSourceGroups.add(LayerGroups::GroupScratch);

        return bindings;
    }

    inline void configure(MeshLibrary& meshLibrary) {
        meshLibrary.setGroupBindings(createGroupBindings());
    }
}
