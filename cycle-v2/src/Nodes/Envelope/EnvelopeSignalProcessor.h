#pragma once

#include "../../Runtime/AudioProcessContextUtils.h"
#include "../Trimesh/TrimeshPanelEnvironment.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

#include <memory>

class VertCube;

namespace CycleV2 {

class EnvelopeSignalProcessor {
public:
    EnvelopeSignalProcessor();
    ~EnvelopeSignalProcessor();

    void process(AudioProcessContext& context) const;

private:
    void initialiseDefaultMesh();
    void addVertex(float x, float y, float curve);
    void setDefaultMorphVariant(int cubeIndex, float redHighAmp, float blueHighAmp);
    void renderEnvelope(AudioProcessContext& context, SignalPayload& output) const;

    static constexpr size_t defaultTraversalColumns = 8;

    TrimeshPanelEnvironment environment;
    std::unique_ptr<EnvelopeMesh> defaultMesh;
    mutable EnvRasterizer rasterizer;
};

}
