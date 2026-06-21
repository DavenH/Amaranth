#include "WaveshaperSignalProcessor.h"

#include "../Effect2D/Effect2DMeshState.h"

#include <Curve/Mesh/Vertex.h>

namespace CycleV2 {

namespace {

constexpr float kWaveshaperPadding = 0.125f;

String parameterValue(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        const String& fallback = {}) {
    for (const auto& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value;
        }
    }

    return fallback;
}

float parameterFloat(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        float fallback) {
    return parameterValue(parameters, id, String(fallback)).getFloatValue();
}

std::vector<Effect2DVertexState> defaultVertices() {
    return {
            { kWaveshaperPadding * 0.5f, kWaveshaperPadding * 0.5f, 1.f },
            { kWaveshaperPadding, kWaveshaperPadding, 1.f },
            { 1.f - kWaveshaperPadding, 1.f - kWaveshaperPadding, 1.f },
            { 1.f - kWaveshaperPadding * 0.5f, 1.f - kWaveshaperPadding * 0.5f, 1.f }
    };
}

}

WaveshaperSignalProcessor::WaveshaperSignalProcessor() {
    rasterizer.setDims(Dimensions(Vertex::Phase, Vertex::Amp));
    rasterizer.setMesh(&mesh);
}

WaveshaperSignalProcessor::~WaveshaperSignalProcessor() {
    mesh.destroy();
}

void WaveshaperSignalProcessor::prepareProcess(const std::vector<NodeParameter>& parameters) {
    syncTransferTable(parameters);
    preGain = parameterFloat(parameters, "pre", 1.f);
    postGain = parameterFloat(parameters, "post", 1.f);
}

void WaveshaperSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    transfer.process(buffer, preGain, postGain);
}

void WaveshaperSignalProcessor::syncTransferTable(const std::vector<NodeParameter>& parameters) {
    const String serializedVertices = parameterValue(parameters, Effect2DMeshState::parameterId());

    if (serializedVertices == lastVertexState && mesh.getNumVerts() > 0) {
        return;
    }

    rebuildMesh(serializedVertices);
    lastVertexState = serializedVertices;
    rasterizer.updateGeometry();
    rasterizer.updateWaveform();
    transfer.rasterizeFrom(rasterizer.sampler(), kWaveshaperPadding);
}

void WaveshaperSignalProcessor::rebuildMesh(const String& serializedVertices) {
    mesh.destroy();

    auto vertices = Effect2DMeshState::parse(serializedVertices);
    if (vertices.empty()) {
        vertices = defaultVertices();
    }

    for (const auto& vertex : vertices) {
        addVertex(vertex.x, vertex.y, vertex.curve);
    }
}

void WaveshaperSignalProcessor::addVertex(float x, float y, float curve) {
    auto* vertex = new Vertex(x, y);
    vertex->values[Vertex::Curve] = curve;

    if (curve >= 1.f) {
        vertex->setMaxSharpness();
    }

    mesh.addVertex(vertex);
}

}
