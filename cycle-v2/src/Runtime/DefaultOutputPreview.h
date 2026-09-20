#pragma once

#include "Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

class DefaultOutputPreview {
public:
    static GraphPreviewResult::SignalProbePreview normalizedTime(
            const GraphPreviewResult::SignalProbePreview& preview);
    static GraphPreviewResult::SignalProbePreview spectrum(
            const GraphPreviewResult::SignalProbePreview& preview);
};

}
