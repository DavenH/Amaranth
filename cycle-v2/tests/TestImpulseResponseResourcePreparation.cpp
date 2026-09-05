#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/ImpulseResponse/ImpulseResponseResourcePreparation.h"

#include <Curve/Curve.h>

#include <algorithm>

using namespace CycleV2;

namespace {

struct TemporaryAudioFile {
    TemporaryAudioFile() = default;
    TemporaryAudioFile(const TemporaryAudioFile&) = delete;
    TemporaryAudioFile& operator=(const TemporaryAudioFile&) = delete;
    TemporaryAudioFile(TemporaryAudioFile&& other) noexcept :
            file(std::move(other.file)) {
        other.file = File();
    }
    ~TemporaryAudioFile() { file.deleteFile(); }

    File file;
};

struct CurveTableScope {
    CurveTableScope() { Curve::calcTable(); }
    ~CurveTableScope() { Curve::deleteTable(); }
};

TemporaryAudioFile writeImpulseWave() {
    TemporaryAudioFile result;
    result.file = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("cycle-v2-ir-resource", ".wav");
    AudioBuffer<float> buffer(1, 512);
    buffer.clear();
    buffer.setSample(0, 0, 0.25f);
    buffer.setSample(0, 100, -0.5f);

    WavAudioFormat format;
    std::unique_ptr<FileOutputStream> stream(result.file.createOutputStream());
    REQUIRE(stream != nullptr);
    std::unique_ptr<AudioFormatWriter> writer(
            format.createWriterFor(stream.release(), 48000.0, 1, 16, {}, 0));
    REQUIRE(writer != nullptr);
    REQUIRE(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()));
    return result;
}

String parameterValue(const Node& node, const String& parameterId) {
    const auto found = std::find_if(
            node.parameters.begin(),
            node.parameters.end(),
            [&](const auto& parameter) { return parameter.id == parameterId; });
    return found != node.parameters.end() ? found->value : String();
}

}

TEST_CASE("IR audio preparation decodes normalizes trims and selects a supported size",
        "[cycle-v2][ir-resource][audio-resource]") {
    const TemporaryAudioFile audio = writeImpulseWave();
    const Node node = GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {});
    PreparedImpulseResponseAudio prepared;

    const Result result = ImpulseResponseResourcePreparation::prepare(
            audio.file,
            ImpulseResponseImportMode::Direct,
            node,
            prepared);

    REQUIRE(result.wasOk());
    REQUIRE(prepared.edit.nodeId == node.id);
    REQUIRE(prepared.edit.mode == "direct");
    REQUIRE(prepared.edit.resource.name == audio.file.getFileName());
    REQUIRE(prepared.edit.resource.sampleRate == Catch::Approx(48000.0));
    REQUIRE(prepared.edit.resource.samples.size() == 101);
    REQUIRE(prepared.edit.resource.samples[100] == Catch::Approx(-1.f).margin(0.001f));
    REQUIRE(prepared.impulseLength == 128);
    REQUIRE(prepared.edit.model == nullptr);
}

TEST_CASE("IR model preparation uses the shared modeller and advances the curve revision",
        "[cycle-v2][ir-resource][audio-resource]") {
    CurveTableScope curveTable;
    const TemporaryAudioFile audio = writeImpulseWave();
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const Node& node = *document.graph().findNode("ir");
    const String originalModel = JSON::toString(node.model->writeJSON(), true);
    const String originalSize = parameterValue(node, "size");
    PreparedImpulseResponseAudio prepared;

    const Result result = ImpulseResponseResourcePreparation::prepare(
            audio.file,
            ImpulseResponseImportMode::Modelled,
            node,
            prepared);
    const auto model = std::dynamic_pointer_cast<const CurveNodeModelState>(prepared.edit.model);

    REQUIRE(result.wasOk());
    REQUIRE(prepared.edit.mode == "modelled");
    REQUIRE(model != nullptr);
    REQUIRE(model->revision() == node.model->revision() + 1);
    REQUIRE(model->flatCurve()->getVertices().size() >= 4);

    REQUIRE(commands.setNodeAudioResource(std::move(prepared.edit)).succeeded());
    const Node& imported = *document.graph().findNode("ir");
    REQUIRE(document.graph().findAudioResourceBinding("ir")->mode == "modelled");
    REQUIRE(parameterValue(imported, "size") != originalSize);
    REQUIRE(JSON::toString(imported.model->writeJSON(), true) != originalModel);

    REQUIRE(document.undo());
    const Node& restored = *document.graph().findNode("ir");
    REQUIRE(document.graph().findAudioResourceBinding("ir") == nullptr);
    REQUIRE(parameterValue(restored, "size") == originalSize);
    REQUIRE(JSON::toString(restored.model->writeJSON(), true) == originalModel);
}

TEST_CASE("IR audio preparation rejects missing and undersized inputs without an edit",
        "[cycle-v2][ir-resource][audio-resource]") {
    const Node node = GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {});
    PreparedImpulseResponseAudio prepared;
    const File missing = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("cycle-v2-missing-ir", ".wav");

    const Result result = ImpulseResponseResourcePreparation::prepare(
            missing,
            ImpulseResponseImportMode::Direct,
            node,
            prepared);

    REQUIRE(result.failed());
    REQUIRE(prepared.edit.resource.samples.empty());
}
