#pragma once

#include <array>
#include <optional>

#include <UI/Panels/ScalarSurfaceMaterial.h>

#include "Graph/PresetPresentation.h"
#include "Runtime/GraphPresentationModel.h"
#include "Runtime/VoicePlayback.h"

namespace CycleV2 {

struct SpyFrameRequest {
    String probeId { "default-output" };
    std::array<int, 128> controllers {};
    std::optional<int> midiNote;
    std::optional<ScalarSurfaceTimeStyle> gradient;
    VoicePlaybackFrame playback;
    int width { 1280 };
    int height { 720 };
};

struct SpyFrameResult {
    Image image;
    int midiNote {};
    ScalarSurfaceTimeStyle gradient {};
    double noteOffTime {};
    uint64_t contentRevision {};
    std::vector<float> playbackPositions;
};

class SpyFrameRenderer {
public:
    static SpyFrameResult render(
            const NodeGraph& graph,
            GraphPresentationModel& presentation,
            const PresetPresentation& metadata,
            const SpyFrameRequest& request,
            String& error);
};

}
