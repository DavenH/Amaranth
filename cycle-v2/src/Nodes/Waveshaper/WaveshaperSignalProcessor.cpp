#include "WaveshaperSignalProcessor.h"

#include "../../Graph/NodeDefinition.h"
#include "../Effect2D/CurveNodeModels.h"
#include "../Effect2D/Effect2DMeshState.h"

#include <Curve/Curve.h>
#include <Curve/Mesh/Vertex.h>
#include <Util/NumberUtils.h>

namespace CycleV2 {

namespace {

constexpr float kWaveshaperPadding = 0.125f;

float cycle1GainForParameter(float value) {
    return (float) NumberUtils::fromDecibels(45.f * (2.f * value - 1.f));
}

void ensureCurveTable() {
    if (Curve::table == nullptr) {
        Curve::calcTable();
    }
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

std::shared_ptr<const WaveshaperConfiguration> WaveshaperSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters) {
    auto result = std::make_shared<WaveshaperConfiguration>();
    result->enabled = typedParameterBool(parameters, "enabled", true);
    auto preparedTransfer = std::make_shared<WaveshaperTransfer>();
    Mesh preparedMesh("CycleV2WaveshaperConfiguration");
    FXRasterizer preparedRasterizer(nullptr, "CycleV2WaveshaperConfigurationRasterizer");
    preparedRasterizer.setDims(Dimensions(Vertex::Phase, Vertex::Amp));
    preparedRasterizer.setMesh(&preparedMesh);

    auto vertices = CurveNodeModelCodec::flatVerticesFromParameters(parameters);
    if (vertices.empty()) {
        vertices = defaultVertices();
    }

    for (const auto& state : vertices) {
        auto* vertex = new Vertex(state.x, state.y);
        vertex->values[Vertex::Curve] = state.curve;
        if (state.curve >= 1.f) {
            vertex->setMaxSharpness();
        }
        preparedMesh.addVertex(vertex);
    }

    ensureCurveTable();
    preparedRasterizer.updateGeometry();
    preparedRasterizer.updateWaveform();
    preparedTransfer->rasterizeFrom(preparedRasterizer.sampler(), kWaveshaperPadding);
    preparedMesh.destroy();

    result->transfer = std::move(preparedTransfer);
    result->preGain = cycle1GainForParameter(parameterFloat(parameters, "pre", 0.5f));
    result->postGain = cycle1GainForParameter(parameterFloat(parameters, "post", 0.5f));
    const int requestedFactor = (int) parameterFloat(parameters, "aaFactor", 1.f);
    result->oversampleFactor = requestedFactor >= 8 ? 8
            : requestedFactor >= 4 ? 4
            : requestedFactor >= 2 ? 2
            : 1;
    return result;
}

void WaveshaperSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    oversampleMemory.resize(spec.maximumFrameCount * 8);
}

void WaveshaperSignalProcessor::adoptConfiguration(const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::Waveshaper) {
        return;
    }

    configuration = std::static_pointer_cast<const WaveshaperConfiguration>(published.value);
    preGain = configuration->preGain;
    postGain = configuration->postGain;
    oversampleFactor = configuration->oversampleFactor;
    oversampler.setOversampleFactor(oversampleFactor);
    adoptedRevision = published.revision;
}

void WaveshaperSignalProcessor::prepareLegacy(
        const std::vector<NodeParameter>& parameters,
        const AudioProcessTiming&) {
    if (configuration != nullptr) {
        return;
    }

    syncTransferTable(parameters);
    preGain = cycle1GainForParameter(parameterFloat(parameters, "pre", 0.5f));
    postGain = cycle1GainForParameter(parameterFloat(parameters, "post", 0.5f));
    const int requestedFactor = (int) parameterFloat(parameters, "aaFactor", 1.f);
    oversampleFactor = requestedFactor >= 8 ? 8
            : requestedFactor >= 4 ? 4
            : requestedFactor >= 2 ? 2
            : 1;
    oversampler.setOversampleFactor(oversampleFactor);
}

void WaveshaperSignalProcessor::beginBlock(size_t frameCount) {
    useOversampling = oversampleFactor > 1;
    if (!useOversampling) {
        return;
    }

    const size_t requiredSize = frameCount * (size_t) oversampleFactor;
    if (oversampleMemory.size() < requiredSize) {
        oversampleMemory.resize(requiredSize);
    }
    oversampler.setMemoryBuffer({ oversampleMemory.data(), (int) oversampleMemory.size() });
}

void WaveshaperSignalProcessor::beginTraversalGrid(size_t, size_t) {
    useOversampling = false;
}

void WaveshaperSignalProcessor::endTraversalGrid() {
    useOversampling = oversampleFactor > 1;
}

void WaveshaperSignalProcessor::processBuffer(Buffer<float> buffer, const SignalProcessPosition&) {
    if (useOversampling) {
        oversampler.startOversamplingBlock(buffer);
    }

    const WaveshaperTransfer& activeTransfer = configuration != nullptr
            ? *configuration->transfer
            : transfer;
    activeTransfer.process(buffer, preGain, postGain);

    if (useOversampling) {
        oversampler.stopOversamplingBlock();
    }
}

void WaveshaperSignalProcessor::syncTransferTable(const std::vector<NodeParameter>& parameters) {
    const String serializedVertices = Effect2DMeshState::serialize(
            CurveNodeModelCodec::flatVerticesFromParameters(parameters));

    if (serializedVertices == lastVertexState && mesh.getNumVerts() > 0) {
        return;
    }

    rebuildMesh(serializedVertices);
    lastVertexState = serializedVertices;
    ensureCurveTable();
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
