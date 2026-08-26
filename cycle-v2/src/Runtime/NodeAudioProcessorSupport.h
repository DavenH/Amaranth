#pragma once

#include "Runtime/AudioProcessTypes.h"
#include "Runtime/UnarySignalProcessor.h"

namespace CycleV2 {

void processPassthrough(AudioProcessContext& context);
void processUnaryEffect(
        IUnarySignalOperation& operation,
        UnarySignalProcessor& processor,
        AudioProcessContext& context,
        bool enabled);

}
