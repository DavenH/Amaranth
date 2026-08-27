#pragma once

#include "Graph/GraphEditor.h"

namespace CycleV2 {

enum class ImpulseResponseImportMode {
    Direct,
    Modelled
};

struct PreparedImpulseResponseAudio {
    NodeAudioResourceEdit edit;
    int impulseLength {};
};

class ImpulseResponseResourcePreparation {
public:
    static Result prepare(
            const File& file,
            ImpulseResponseImportMode mode,
            const Node& node,
            PreparedImpulseResponseAudio& output);
};

}
