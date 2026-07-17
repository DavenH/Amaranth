#pragma once

#include "AudioProcessTypes.h"
#include "UnarySignalProcessor.h"

namespace CycleV2 {

void processPassthrough(AudioProcessContext& context);
void processUnaryEffect(
        IUnarySignalOperation& operation,
        UnarySignalProcessor& processor,
        AudioProcessContext& context,
        bool enabled);

}
