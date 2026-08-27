#include "Nodes/ImpulseResponse/ImpulseResponseResourcePreparation.h"

#include "Nodes/Curve/Model/CurveNodeModels.h"

#include <Algo/AutoModeller.h>
#include <Audio/CycleDsp/IrModel.h>
#include <Audio/PitchedSample.h>
#include <Util/NumberUtils.h>

namespace CycleV2 {

namespace {

constexpr float kIrPadding = 0.0625f;
constexpr int kMinimumIrSampleCount = 64;
constexpr int kMinimumIrImpulseLength = 128;
constexpr int kMaximumIrImpulseLength = 16384;

NodeModelStatePtr buildModelledCurve(
        const std::vector<float>& samples,
        int impulseLength,
        const Node& node) {
    std::vector<float> modellingSamples(samples);
    Buffer<float> buffer(modellingSamples.data(), (int) modellingSamples.size());
    buffer.mul(0.7f);

    AutoModeller modeller;
    const auto intercepts = modeller.modelToIntercepts(
            buffer,
            false,
            kIrPadding,
            0.1f);
    if (intercepts.size() < 2) {
        return {};
    }

    const float scaleRatio = (float) samples.size() / (float) impulseLength;
    std::vector<FlatCurveVertex> vertices;
    vertices.reserve(intercepts.size() + 2);
    CurveVertexId identity = 1;
    for (const auto& intercept : intercepts) {
        vertices.push_back({
                identity++,
                jlimit(0.f, 1.f, (intercept.x - kIrPadding) * scaleRatio + kIrPadding),
                jlimit(0.f, 1.f, intercept.y * 0.5f + 0.5f),
                jlimit(0.f, 1.f, intercept.shp)
        });
    }
    vertices.push_back({
            identity++,
            (kIrPadding - 0.0001f - kIrPadding) * scaleRatio + kIrPadding,
            0.5f,
            0.f
    });
    vertices.push_back({
            identity,
            (kIrPadding - 0.01f - kIrPadding) * scaleRatio + kIrPadding,
            0.5f,
            0.f
    });

    FlatCurveModel curve("CycleV2ImpulseResponseCurve");
    if (!curve.replaceVertices(std::move(vertices))) {
        return {};
    }
    const uint64_t currentRevision = node.model != nullptr ? node.model->revision() : 0;
    const uint64_t nextRevision = currentRevision + 1;
    curve.setPublicationRevision(nextRevision);
    const auto currentCurve = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    const var editorState = currentCurve != nullptr ? currentCurve->editorJSON().clone() : var();
    return CurveNodeModelState::copyOf(curve, nextRevision, editorState);
}

}

Result ImpulseResponseResourcePreparation::prepare(
        const File& file,
        ImpulseResponseImportMode mode,
        const Node& node,
        PreparedImpulseResponseAudio& output) {
    PitchedSample decoded;
    const int loadResult = decoded.load(file.getFullPathName());
    if (loadResult < 0) {
        return Result::fail("The selected audio file could not be decoded.");
    }
    if (decoded.audio.size() < kMinimumIrSampleCount) {
        return Result::fail("Impulse audio must contain at least 64 samples.");
    }

    const int sampleCount = CycleDsp::irTrimmedSampleCount(decoded.audio.left);
    std::vector<float> samples((size_t) sampleCount);
    VecOps::copy(decoded.audio.left.get(), samples.data(), sampleCount);
    const int impulseLength = jlimit(
            kMinimumIrImpulseLength,
            kMaximumIrImpulseLength,
            NumberUtils::nextPower2((unsigned) sampleCount));

    NodeAudioResourceEdit edit;
    edit.nodeId = node.id;
    edit.resource = {
            Uuid().toString(),
            file.getFileName(),
            (double) decoded.samplerate,
            std::move(samples)
    };
    edit.mode = mode == ImpulseResponseImportMode::Direct ? "direct" : "modelled";
    edit.parameters.push_back({
            "size",
            "Size",
            String(CycleDsp::irImpulseLengthValue(impulseLength), 9)
    });
    edit.expectedModelRevision = node.model != nullptr ? node.model->revision() : 0;
    if (mode == ImpulseResponseImportMode::Modelled) {
        edit.model = buildModelledCurve(edit.resource.samples, impulseLength, node);
        if (edit.model == nullptr) {
            return Result::fail("The impulse could not be converted to an editable curve.");
        }
    }

    output = { std::move(edit), impulseLength };
    return Result::ok();
}

}
