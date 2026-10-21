#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "App/ImpulseResponseImportAutomation.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/ImpulseResponse/ImpulseResponseAnalysis.h"
#include "Nodes/ImpulseResponse/ImpulseResponseResourcePreparation.h"

#include <Curve/Curve.h>

#include <algorithm>
#include <cmath>

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

var importRequest(const File& file, const String& mode) {
    auto request = var(new DynamicObject());
    request.getDynamicObject()->setProperty("nodeId", "ir");
    request.getDynamicObject()->setProperty("path", file.getFullPathName());
    request.getDynamicObject()->setProperty("mode", mode);
    return request;
}

ImpulseResponseSource importedImpulse(const NodeGraph& graph) {
    const Node& node = *graph.findNode("ir");
    const auto& binding = *graph.findAudioResourceBinding("ir");
    const auto impulse = prepareImpulseResponseSource(node.parameters, node.model,
            binding.mode == "direct" ? graph.findAudioResource(binding.resourceId) : nullptr);
    REQUIRE(impulse.has_value());
    return *impulse;
}

void requirePortableImpulse(const std::vector<float>& loaded, const std::vector<float>& original) {
    REQUIRE(loaded.size() == original.size());
    float maximumError = 0.f;
    for (size_t index = 0; index < loaded.size(); ++index) {
        maximumError = std::max(maximumError, std::abs(loaded[index] - original[index]));
    }
    // Graph persistence rounds curve coordinates to five decimal places.
    REQUIRE(maximumError < 0.0002f);
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

TEST_CASE("IR import automation produces a portable audible kernel in one undoable edit",
        "[cycle-v2][ir-resource][ir-import-automation]") {
    CurveTableScope curveTable;
    for (const String mode : { String("direct"), String("modelled") }) {
        DYNAMIC_SECTION(mode) {
            const TemporaryAudioFile audio = writeImpulseWave();
            NodeGraph graph;
            graph.addNode(GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {}));
            GraphDocument document(std::move(graph));
            GraphCommandDispatcher commands(document);
            const String before = document.toJson();
            const Node& initialNode = *document.graph().findNode("ir");
            const String originalPost = parameterValue(initialNode, "post");
            const String originalHighPass = parameterValue(initialNode, "highPass");
            const uint64_t originalModelRevision = initialNode.model->revision();
            String error;

            const var result = ImpulseResponseImportAutomation::apply(
                    document, commands, importRequest(audio.file, mode), audio.file, error);

            REQUIRE(error.isEmpty());
            REQUIRE(result["mode"].toString() == mode);
            REQUIRE((int) result["resourceSampleCount"] == 101);
            REQUIRE((double) result["sourceSampleRate"] == Catch::Approx(48000.0));
            REQUIRE((int) result["impulseLength"] == 128);
            REQUIRE((int64) result["documentRevision"] == (int64) document.revision());
            REQUIRE(result["parameters"]["post"].toString() == originalPost);
            REQUIRE(result["parameters"]["highPass"].toString() == originalHighPass);
            const auto impulse = importedImpulse(document.graph());
            REQUIRE(impulse.rawImpulse.size() == 128);
            REQUIRE(std::any_of(impulse.rawImpulse.begin(), impulse.rawImpulse.end(),
                    [](float sample) { return std::abs(sample) > 0.1f; }));
            if (mode == "direct") {
                REQUIRE(impulse.rawImpulse[0] == Catch::Approx(0.5f).margin(0.001f));
                REQUIRE(impulse.rawImpulse[100] == Catch::Approx(-1.f).margin(0.001f));
                REQUIRE((int64) result["modelRevision"] == (int64) originalModelRevision);
            } else {
                REQUIRE((int) result["modelVertexCount"] >= 4);
                REQUIRE((int64) result["modelRevision"] == (int64) originalModelRevision + 1);
            }

            const String after = document.toJson();
            REQUIRE(audio.file.deleteFile());
            const GraphLoadResult loaded = GraphSerializer().loadJsonString(after);
            REQUIRE(loaded.succeeded());
            const auto reloadedImpulse = importedImpulse(loaded.graph);
            requirePortableImpulse(reloadedImpulse.rawImpulse, impulse.rawImpulse);
            REQUIRE(GraphSerializer().toJsonString(loaded.graph)
                    == GraphSerializer().toJsonString(document.graph()));
            REQUIRE(document.undo());
            REQUIRE(document.toJson() == before);
            REQUIRE_FALSE(document.undo());
            REQUIRE(document.redo());
            REQUIRE(document.toJson() == after);
            REQUIRE(importedImpulse(document.graph()).rawImpulse == impulse.rawImpulse);
        }
    }
}

TEST_CASE("IR import automation rejects invalid requests and active gestures atomically",
        "[cycle-v2][ir-resource][ir-import-automation]") {
    const TemporaryAudioFile audio = writeImpulseWave();
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {}));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "other", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const String before = document.toJson();
    const uint64_t revision = document.revision();
    var request = importRequest(audio.file, "direct");
    File source = audio.file;

    SECTION("mode is explicit") {
        request.getDynamicObject()->removeProperty("mode");
    }
    SECTION("mode is recognized") {
        request.getDynamicObject()->setProperty("mode", "unknown");
    }
    SECTION("path is required") {
        request.getDynamicObject()->removeProperty("path");
    }
    SECTION("node exists") {
        request.getDynamicObject()->setProperty("nodeId", "missing");
    }
    SECTION("node supports impulse audio") {
        request.getDynamicObject()->setProperty("nodeId", "other");
    }
    SECTION("source exists") {
        source = audio.file.getSiblingFile("missing-impulse.wav");
    }
    SECTION("source decodes") {
        REQUIRE(audio.file.replaceWithText("This is not an audio file."));
    }
    SECTION("gesture is preserved") {
        commands.beginTransientEdit();
    }

    const bool hadGesture = commands.hasTransientEdit();
    String error;
    const var result = ImpulseResponseImportAutomation::apply(document, commands, request, source, error);

    REQUIRE(error.isNotEmpty());
    REQUIRE(result.isVoid());
    REQUIRE(document.toJson() == before);
    REQUIRE(document.revision() == revision);
    REQUIRE(commands.hasTransientEdit() == hadGesture);
    REQUIRE_FALSE(document.undo());
    commands.cancelTransientEdit();
}
