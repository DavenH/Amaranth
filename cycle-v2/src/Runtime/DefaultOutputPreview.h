#pragma once

#include "Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

class DefaultOutputPreview {
public:
    static GraphPreviewResult::SignalProbePreview forView(
            const GraphPreviewResult::SignalProbePreview& preview, PresetPreviewView view);
    static GraphPreviewResult::SignalProbePreview spectrum(
            const GraphPreviewResult::SignalProbePreview& preview);
};

}
