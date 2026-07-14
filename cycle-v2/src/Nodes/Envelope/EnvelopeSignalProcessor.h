#pragma once

#include "../../Runtime/AudioProcessContextUtils.h"
#include "../../Runtime/NodeDspConfiguration.h"
#include "EnvelopeMeshState.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

namespace CycleV2 {

struct EnvelopeConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::Envelope; }

    std::shared_ptr<EnvelopeMesh> mesh;
    std::shared_ptr<EnvRasterizer> rasterizer;
    float level { 1.f };
    bool logarithmic {};
};

class EnvelopeSignalProcessor {
public:
    EnvelopeSignalProcessor();
    ~EnvelopeSignalProcessor();

    static std::shared_ptr<const EnvelopeConfiguration> buildConfiguration(
            const std::vector<NodeParameter>& parameters);

    void prepareExecution(const AudioExecutionSpec& spec);
    void adoptConfiguration(const PublishedNodeConfiguration& published);

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
    float level { 1.f };
    uint64_t adoptedRevision {};
    uint64_t pendingRevision {};
    std::shared_ptr<const EnvelopeConfiguration> configuration;
};

}
