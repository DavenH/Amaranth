#pragma once

#include "../../Runtime/AudioProcessContextUtils.h"
#include "EnvelopeMeshState.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

namespace CycleV2 {

class EnvelopeSignalProcessor {
public:
    EnvelopeSignalProcessor();
    ~EnvelopeSignalProcessor();

    void process(AudioProcessContext& context);

private:
    bool syncModel(const std::vector<NodeParameter>& parameters);
    void applyLifecycleEvent(const NoteLifecycleEvent& event);
    void renderSegment(Buffer<float> output, size_t start, size_t count, const AudioProcessTiming& timing);

    static constexpr size_t defaultTraversalColumns = 8;

    EnvelopeMesh mesh;
    EnvRasterizer rasterizer;
    MeshLibrary::EnvProps props;
    String snapshotState;
    float redMorph { -1.f };
    float blueMorph { -1.f };
    bool active {};
    bool modelReady {};
};

}
