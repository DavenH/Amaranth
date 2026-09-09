#pragma once

#include "Nodes/Envelope/EnvelopeConfiguration.h"

namespace CycleV2 {

class CycleEnvelopePlaybackSource {
public:
    virtual ~CycleEnvelopePlaybackSource() = default;

    virtual const EnvelopeConfiguration* cycleEnvelopeConfiguration() const = 0;
};

}
