#include "EnvelopeSignalProcessor.h"

#include <Curve/Curve.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Mesh/VertCube.h>

namespace CycleV2 {

EnvelopeSignalProcessor::EnvelopeSignalProcessor() :
        defaultMesh (std::make_unique<EnvelopeMesh>("CycleV2RuntimeEnvelope"))
    ,   rasterizer  (&environment.getRepo(), nullptr, "CycleV2RuntimeEnvelopeRasterizer") {
    initialiseDefaultMesh();
    rasterizer.setMesh(defaultMesh.get());
}

EnvelopeSignalProcessor::~EnvelopeSignalProcessor() {
    if (defaultMesh != nullptr) {
        defaultMesh->destroy();
    }
}

void EnvelopeSignalProcessor::process(AudioProcessContext& context) const {
    auto output = makeOutputPayload(context, 0);
    const float level = parameterFloat(context.parameters, "level", 1.f);

    renderEnvelope(context, output);
    payloadBuffer(output, context.frameCount).mul(level);
    publishVectorAsTraversalGrid(output, defaultTraversalColumns, context.workArena);
    publishSingleOutput(context, std::move(output));
}

void EnvelopeSignalProcessor::initialiseDefaultMesh() {
    addVertex(0.f, 0.f, 1.f);
    addVertex(0.05f, 1.f, 0.5f);
    addVertex(0.7f, 0.8f, 0.3f);
    setDefaultMorphVariant(2, 0.42f, 0.94f);
    addVertex(0.999f, 0.8f, 1.f);
    defaultMesh->setSustainToLast();
    addVertex(1.075f, 0.6f, -1.f);
    addVertex(1.15f, 0.f, -1.f);
    addVertex(1.25f, 0.f, -1.f);
}

void EnvelopeSignalProcessor::addVertex(float x, float y, float curve) {
    auto* cube = new VertCube(defaultMesh.get());
    MorphPosition position;
    position.time.setValueDirect(0.f);
    position.timeDepth = 0.001f;
    position.red.setValueDirect(0.f);
    position.redDepth = 1.f;
    position.blue.setValueDirect(0.f);
    position.blueDepth = 1.f;
    cube->initVerts(position);

    for (int i = 0; i < (int) VertCube::numVerts; ++i) {
        Vertex* vertex = cube->getVertex(i);
        vertex->values[Vertex::Phase] = x;
        vertex->values[Vertex::Amp] = y;

        if (curve >= 0.f) {
            vertex->values[Vertex::Curve] = curve;
        }
    }

    defaultMesh->addCube(cube);
}

void EnvelopeSignalProcessor::setDefaultMorphVariant(int cubeIndex, float redHighAmp, float blueHighAmp) {
    if (!isPositiveAndBelow(cubeIndex, defaultMesh->getCubes().size())) {
        return;
    }

    VertCube* cube = defaultMesh->getCubes()[(size_t) cubeIndex];
    if (cube == nullptr) {
        return;
    }

    for (int i = 0; i < (int) VertCube::numVerts; ++i) {
        Vertex* vertex = cube->getVertex(i);
        if (vertex == nullptr) {
            continue;
        }

        if (vertex->values[Vertex::Red] > 0.5f) {
            vertex->values[Vertex::Amp] = redHighAmp;
        }

        if (vertex->values[Vertex::Blue] > 0.5f) {
            vertex->values[Vertex::Amp] = blueHighAmp;
        }
    }
}

void EnvelopeSignalProcessor::renderEnvelope(AudioProcessContext& context, SignalPayload& output) const {
    if (context.frameCount == 0) {
        return;
    }

    if (Curve::table == nullptr) {
        Curve::calcTable();
    }

    rasterizer.setMesh(defaultMesh.get());
    rasterizer.setLimits(0.f, 1.25f);
    rasterizer.setWrapsEnds(false);
    rasterizer.setMorphPosition(MorphPosition(
            0.f,
            parameterFloat(context.parameters, "red", 0.f),
            parameterFloat(context.parameters, "blue", 0.f)));
    rasterizer.updateWaveform(defaultMesh.get(), 0.f);

    Buffer<float> dest = payloadBuffer(output, context.frameCount);
    const auto sampler = rasterizer.sampler();

    if (!sampler.isSampleable()) {
        dest.set(1.f);
        return;
    }

    const auto snapshot = rasterizer.snapshotView();
    Buffer<float> waveX = snapshot.waveX();
    const float xMinimum = waveX.size() > 0 ? waveX[0] : 0.f;
    const float xMaximum = waveX.size() > 0 ? waveX[waveX.size() - 1] : 1.f;
    const float denominator = context.frameCount > 1 ? (float) (context.frameCount - 1) : 1.f;
    int currentIndex = snapshot.zeroIndex();

    for (int i = 0; i < dest.size(); ++i) {
        const float x = jmap((float) i / denominator, xMinimum, xMaximum - 1.0e-5f);
        dest[i] = sampler.sampleAt(x, currentIndex);
    }
}

}
