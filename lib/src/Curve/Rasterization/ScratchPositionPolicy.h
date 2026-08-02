#pragma once

#include <Curve/Mesh/Vertex.h>
#include <Obj/MorphPosition.h>

namespace Rasterization {

enum class ScratchSourceDomain {
    Unsupported,
    Time,
    Spectral
};

class ScratchPositionPolicy {
public:
    static bool shouldApply(ScratchSourceDomain domain, int primaryViewAxis) {
        return domain == ScratchSourceDomain::Spectral
                || (domain == ScratchSourceDomain::Time && primaryViewAxis == Vertex::Time);
    }

    static MorphPosition resolve(
            const MorphPosition& fallback,
            ScratchSourceDomain domain,
            int primaryViewAxis,
            float scratchPosition) {
        MorphPosition result = fallback;
        if (shouldApply(domain, primaryViewAxis)) {
            result.time.setValueDirect(juce::jlimit(0.f, 1.f, scratchPosition));
        }
        return result;
    }
};

}
