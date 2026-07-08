#pragma once

#include "../../Runtime/AudioProcessContextUtils.h"

namespace CycleV2 {

class EnvelopeSignalProcessor {
public:
    void process(AudioProcessContext& context) const;

private:
    static constexpr size_t defaultTraversalColumns = 8;
};

}
