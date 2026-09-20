#pragma once

#include "Graph/PresetPresentation.h"
#include "Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

class PresetPreviewGenerator {
public:
    static GraphPreviewResult::SignalProbePreview forView(
            const GraphPreviewResult::SignalProbePreview& timePreview,
            PresetPreviewView view);
    static PresetPreviewImage encodeJpeg(
            const GraphPreviewResult::SignalProbePreview& timePreview,
            PresetPreviewView view,
            int width = 320,
            int height = 180);
};

}
