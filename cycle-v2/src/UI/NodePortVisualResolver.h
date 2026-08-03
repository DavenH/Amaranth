#pragma once

#include "../Graph/NodeGraph.h"

namespace CycleV2 {

enum class PortVisualSemantic {
    None,
    ModulationYrb,
    ModulationRb,
    PitchEnvelope,
    UnisonConfiguration,
    VoiceContext,
    ScratchAttachment
};

class NodePortVisualResolver {
public:
    static PortVisualSemantic semanticFor(const Port& port);
    static PortVisualSemantic modulationSemantic(bool includesYellow);
    static Colour colourFor(PortDomain domain);
};

}
