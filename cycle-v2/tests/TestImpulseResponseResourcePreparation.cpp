#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "App/ImpulseResponseImportAutomation.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Effects/EffectSignalProcessors.h"
#include "Nodes/ImpulseResponse/ImpulseResponseAnalysis.h"
#include "Nodes/ImpulseResponse/ImpulseResponseResourcePreparation.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Runtime/OfflineGraphAudioRenderer.h"

#include <Curve/Curve.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

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

TemporaryAudioFile writeResonantWave(double sampleRate) {
    TemporaryAudioFile result;
    result.file = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("cycle-v2-ir-resonance", ".wav");
    AudioBuffer<float> buffer(1, (int) (sampleRate * 0.1));
    for (int index = 0; index < buffer.getNumSamples(); ++index) {
        const double seconds = (double) index / sampleRate;
        buffer.setSample(0, index, (float) (0.8 * std::exp(-seconds / 0.018)
                * std::sin(MathConstants<double>::twoPi * 600.0 * seconds)));
    }
    WavAudioFormat format;
    std::unique_ptr<FileOutputStream> stream(result.file.createOutputStream());
    REQUIRE(stream != nullptr);
    std::unique_ptr<AudioFormatWriter> writer(
            format.createWriterFor(stream.release(), sampleRate, 1, 24, {}, 0));
    REQUIRE(writer != nullptr);
    REQUIRE(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()));
    return result;
}

double modalFrequency(const std::vector<float>& impulse, double sampleRate) {
    std::vector<double> crossings;
    const int end = std::min((int) impulse.size(), (int) (sampleRate * 0.055));
    for (int index = (int) (sampleRate * 0.01); index < end; ++index) {
        const float first = impulse[(size_t) index - 1];
        const float second = impulse[(size_t) index];
        if (first < 0.f && second >= 0.f) {
            crossings.push_back((double) index - 1.0 - first / (second - first));
        }
    }
    REQUIRE(crossings.size() > 20);
    return (double) (crossings.size() - 1) * sampleRate / (crossings.back() - crossings.front());
}

double impulseRms(const std::vector<float>& impulse, double sampleRate, double start, double end) {
    const int first = (int) std::round(sampleRate * start);
    const int last = (int) std::round(sampleRate * end);
    REQUIRE(last <= (int) impulse.size());
    double energy = 0.0;
    for (int index = first; index < last; ++index) {
        const double sample = impulse[(size_t) index];
        energy += sample * sample;
    }
    return std::sqrt(energy / (last - first));
}

double modalDecaySeconds(const std::vector<float>& impulse, double sampleRate) {
    return 0.030 / std::log(impulseRms(impulse, sampleRate, 0.010, 0.025)
            / impulseRms(impulse, sampleRate, 0.040, 0.055));
}

double modalResponse(const std::vector<float>& impulse, double sampleRate, double frequency = 600.0) {
    std::complex<double> response;
    for (size_t index = 0; index < impulse.size(); ++index) {
        const double phase = -MathConstants<double>::twoPi * frequency * (double) index / sampleRate;
        response += (double) impulse[index] * std::complex<double>(std::cos(phase), std::sin(phase));
    }
    return std::abs(response);
}

std::optional<std::vector<float>> playbackImpulse(
        const NodeGraph& graph,
        double sampleRate,
        IrSignalProcessor* retainedProcessor = nullptr) {
    const Node& node = *graph.findNode("ir");
    AudioExecutionSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumFrameCount = 256;
    const auto configuration = std::dynamic_pointer_cast<const IrConfiguration>(NodeDspConfigurationFactory().create(
            AudioModuleRole::ImpulseResponse, node.parameters, node.model, spec, &graph, node.id));
    if (configuration == nullptr) {
        return {};
    }
    IrSignalProcessor localProcessor;
    auto& processor = retainedProcessor != nullptr ? *retainedProcessor : localProcessor;
    processor.adoptConfiguration({ 1, "canonical-test", configuration });
    processor.prepareExecution(spec);
    const int sampleCount = (int) std::ceil(configuration->impulse.size()
            * sampleRate / impulseResponseReferenceSampleRate);
    std::vector<float> response((size_t) sampleCount);
    std::vector<float> block(spec.maximumFrameCount);
    for (int start = 0; start < sampleCount; start += (int) block.size()) {
        std::fill(block.begin(), block.end(), 0.f);
        if (start == 0) {
            block.front() = 1.f;
        }
        processor.beginBlock(block.size());
        processor.processBuffer({ block.data(), (int) block.size() }, {});
        VecOps::copy(block.data(), response.data() + start,
                jmin((int) block.size(), sampleCount - start));
    }
    return response;
}

double steadyToneAmplitude(const std::vector<float>& samples, double sampleRate, double frequency) {
    const int first = (int) std::round(sampleRate * 0.2);
    const int last = (int) std::round(sampleRate * 0.3);
    std::complex<double> coefficient;
    for (int index = first; index < last; ++index) {
        const double window = 0.5 - 0.5 * std::cos(MathConstants<double>::twoPi
                * (double) (index - first) / (double) (last - first - 1));
        const double phase = -MathConstants<double>::twoPi * frequency * index / sampleRate;
        coefficient += (double) samples[(size_t) index] * window
                * std::complex<double>(std::cos(phase), std::sin(phase));
    }
    return std::abs(coefficient);
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
            REQUIRE((double) result["canonicalSampleRate"] == Catch::Approx(44100.0));
            REQUIRE((int) result["impulseLength"] == 128);
            REQUIRE((int64) result["documentRevision"] == (int64) document.revision());
            REQUIRE(result["parameters"]["post"].toString() == originalPost);
            REQUIRE(result["parameters"]["highPass"].toString() == originalHighPass);
            const auto impulse = importedImpulse(document.graph());
            REQUIRE(impulse.rawImpulse.size() == 128);
            REQUIRE(std::any_of(impulse.rawImpulse.begin(), impulse.rawImpulse.end(),
                    [](float sample) { return std::abs(sample) > 0.1f; }));
            if (mode == "direct") {
                const auto& resource = *document.graph().findAudioResource(
                        document.graph().findAudioResourceBinding("ir")->resourceId);
                REQUIRE(resource.samples[0] == Catch::Approx(0.5f).margin(0.001f));
                REQUIRE(resource.samples[100] == Catch::Approx(-1.f).margin(0.001f));
                const auto minimum = std::min_element(impulse.rawImpulse.begin(), impulse.rawImpulse.end());
                REQUIRE(std::distance(impulse.rawImpulse.begin(), minimum) >= 91);
                REQUIRE(std::distance(impulse.rawImpulse.begin(), minimum) <= 95);
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

TEST_CASE("IR imports preserve physical resonance decay and gain across playback rates",
        "[cycle-v2][ir-resource][ir-sample-rate]") {
    CurveTableScope curveTable;
    for (const double sourceRate : { 44100.0, 48000.0, 96000.0 }) {
        for (const auto mode : { ImpulseResponseImportMode::Direct, ImpulseResponseImportMode::Modelled }) {
            DYNAMIC_SECTION("source " << sourceRate << " mode " << (int) mode) {
                const TemporaryAudioFile audio = writeResonantWave(sourceRate);
                NodeGraph graph;
                Node node = GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {});
                for (auto& parameter : node.parameters) {
                    if (parameter.id == "highPass") {
                        parameter.value = "0";
                    }
                }
                graph.addNode(std::move(node));
                GraphDocument document(std::move(graph));
                GraphCommandDispatcher commands(document);
                PreparedImpulseResponseAudio prepared;
                REQUIRE(ImpulseResponseResourcePreparation::prepare(audio.file, mode,
                        *document.graph().findNode("ir"), prepared).wasOk());
                REQUIRE(prepared.impulseLength == 8192);
                const auto retainedSamples = prepared.edit.resource.samples;
                REQUIRE(prepared.edit.resource.sampleRate == sourceRate);
                REQUIRE(commands.setNodeAudioResource(std::move(prepared.edit)).succeeded());
                const auto canonical = playbackImpulse(document.graph(), 44100.0);
                REQUIRE(canonical.has_value());
                const double referenceGain = modalResponse(*canonical, 44100.0);
                REQUIRE(referenceGain > 1.0);
                const auto reloaded = GraphSerializer().loadJsonString(document.toJson());
                REQUIRE(reloaded.succeeded());
                REQUIRE(audio.file.deleteFile());
                const auto& resource = *reloaded.graph.findAudioResource(
                        reloaded.graph.findAudioResourceBinding("ir")->resourceId);
                REQUIRE(resource.sampleRate == sourceRate);
                REQUIRE(resource.samples.size() == retainedSamples.size());
                float retainedError = 0.f;
                for (size_t index = 0; index < retainedSamples.size(); ++index) {
                    retainedError = std::max(retainedError,
                            std::abs(resource.samples[index] - retainedSamples[index]));
                }
                REQUIRE(retainedError <= 0.0000051f);
                for (const double playbackRate : { 44100.0, 48000.0, 96000.0 }) {
                    const auto configuration = playbackImpulse(document.graph(), playbackRate);
                    const auto loaded = playbackImpulse(reloaded.graph, playbackRate);
                    REQUIRE(configuration.has_value());
                    REQUIRE(loaded.has_value());
                    REQUIRE(modalFrequency(*configuration, playbackRate)
                            == Catch::Approx(600.0).margin(3.0));
                    REQUIRE(modalDecaySeconds(*configuration, playbackRate)
                            == Catch::Approx(0.018).margin(0.002));
                    for (const double frequency : { 0.0, 200.0, 600.0, 3000.0 }) {
                        REQUIRE(modalResponse(*configuration, playbackRate, frequency)
                                == Catch::Approx(modalResponse(*canonical, 44100.0, frequency)).epsilon(0.025));
                    }
                    REQUIRE(modalFrequency(*loaded, playbackRate)
                            == Catch::Approx(modalFrequency(*configuration, playbackRate)).margin(0.5));
                    REQUIRE(modalDecaySeconds(*loaded, playbackRate)
                            == Catch::Approx(modalDecaySeconds(*configuration, playbackRate)).margin(0.0003));
                }
                if (mode == ImpulseResponseImportMode::Modelled) {
                    const NodeGraph beforeUnload = document.graph();
                    REQUIRE(commands.removeNodeAudioResource("ir").succeeded());
                    REQUIRE(document.graph().getAudioResources().empty());
                    for (const double rate : { 44100.0, 48000.0, 96000.0 }) {
                        REQUIRE(*playbackImpulse(document.graph(), rate)
                                == *playbackImpulse(beforeUnload, rate));
                    }
                }
            }
        }
    }
}

TEST_CASE("IR rate preparation rejects unsupported persisted and execution rates safely",
        "[cycle-v2][ir-resource][ir-sample-rate]") {
    CurveTableScope curveTable;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {}));
    REQUIRE(graph.addAudioResource({ "corrupt-rate", "Corrupt.wav", 1.0e300, { 1.f, 0.f } }));
    REQUIRE(graph.bindAudioResource({ "ir", "corrupt-rate", "direct" }));
    const auto loaded = GraphSerializer().loadJsonString(GraphSerializer().toJsonString(graph));
    REQUIRE(loaded.succeeded());
    REQUIRE_FALSE(playbackImpulse(loaded.graph, 48000.0).has_value());
    NodeGraph unbound;
    unbound.addNode(GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {}));
    REQUIRE(playbackImpulse(unbound, 44100.0).has_value());
    REQUIRE_FALSE(playbackImpulse(unbound, 1.0e300).has_value());
    REQUIRE_FALSE(playbackImpulse(unbound, 7999.0).has_value());
    REQUIRE(resampleImpulseResponse({ 1.f }, 44100.0, 48000.0,
            std::numeric_limits<int>::max()).empty());
    REQUIRE(isSupportedImpulseResponseSampleRate(8000.0));
    REQUIRE(isSupportedImpulseResponseSampleRate(384000.0));
}

TEST_CASE("Retained IR execution prepares the actual playback rate without republishing",
        "[cycle-v2][ir-resource][ir-sample-rate]") {
    CurveTableScope curveTable;
    const TemporaryAudioFile audio = writeResonantWave(48000.0);
    NodeGraph graph;
    auto node = GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {});
    for (auto& parameter : node.parameters) {
        if (parameter.id == "highPass") {
            parameter.value = "0";
        }
    }
    graph.addNode(std::move(node));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    PreparedImpulseResponseAudio prepared;
    REQUIRE(ImpulseResponseResourcePreparation::prepare(audio.file, ImpulseResponseImportMode::Direct,
            *document.graph().findNode("ir"), prepared).wasOk());
    REQUIRE(commands.setNodeAudioResource(std::move(prepared.edit)).succeeded());
    IrSignalProcessor processor;
    for (const double rate : { 44100.0, 96000.0, 48000.0, 44100.0 }) {
        const auto response = playbackImpulse(document.graph(), rate, &processor);
        REQUIRE(response.has_value());
        REQUIRE(modalFrequency(*response, rate) == Catch::Approx(600.0).margin(3.0));
        REQUIRE(modalDecaySeconds(*response, rate) == Catch::Approx(0.018).margin(0.002));
    }
}

TEST_CASE("Compiled realtime IR playback retains its fixed physical resonance across output rates",
        "[cycle-v2][ir-resource][ir-sample-rate][realtime-audio]") {
    CurveTableScope curveTable;
    const TemporaryAudioFile audio = writeResonantWave(48000.0);
    NodeGraph dryGraph = NodeGraph::createDemoGraph();
    auto mesh = TrimeshMeshFactory::createDefaultMesh("IrRateTone");
    const auto* wave = dryGraph.findNode("waveMesh");
    REQUIRE(wave != nullptr);
    REQUIRE(GraphNodeStateEditor().replaceNodeModel(dryGraph, "waveMesh", wave->model->revision(),
            TrimeshNodeModelState::copyOf(*mesh, wave->model->revision() + 1)).succeeded());
    mesh->destroy();
    dryGraph.removeNode("env");
    dryGraph.removeNode("multiply");
    dryGraph.addEdge({ "ifft", "time", "voiceOutput", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal });
    const auto dryCompiled = GraphCompiler().compile(dryGraph);
    REQUIRE(dryCompiled.succeeded());

    for (const auto mode : { ImpulseResponseImportMode::Direct, ImpulseResponseImportMode::Modelled }) {
        DYNAMIC_SECTION("mode " << (int) mode) {
            NodeGraph wetGraph = dryGraph;
            auto ir = GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {});
            for (auto& parameter : ir.parameters) {
                if (parameter.id == "highPass") {
                    parameter.value = "0";
                } else if (parameter.id == "post") {
                    parameter.value = "0";
                }
            }
            wetGraph.addNode(std::move(ir));
            REQUIRE(wetGraph.removeEdge({ "ifft", "time", "voiceOutput", "time",
                    PortDomain::TimeSignal, ConnectionKind::Signal }));
            wetGraph.addEdge({ "ifft", "time", "ir", "time",
                    PortDomain::TimeSignal, ConnectionKind::Signal });
            wetGraph.addEdge({ "ir", "time", "voiceOutput", "time",
                    PortDomain::TimeSignal, ConnectionKind::Signal });
            GraphDocument document(std::move(wetGraph));
            GraphCommandDispatcher commands(document);
            PreparedImpulseResponseAudio prepared;
            REQUIRE(ImpulseResponseResourcePreparation::prepare(audio.file, mode,
                    *document.graph().findNode("ir"), prepared).wasOk());
            REQUIRE(commands.setNodeAudioResource(std::move(prepared.edit)).succeeded());
            const auto wetCompiled = GraphCompiler().compile(document.graph());
            REQUIRE(wetCompiled.succeeded());
            const Node& node = *document.graph().findNode("ir");
            const auto canonical = IrSignalProcessor::buildConfiguration(node.parameters, node.model,
                    IrSignalProcessor::directResource(&document.graph(), node.id));
            REQUIRE(canonical != nullptr);
            const double frequency = MidiMessage::getMidiNoteInHertz(74);
            const double expectedGain = modalResponse(canonical->impulse, 44100.0, frequency)
                    * canonical->postGain;
            for (const double rate : { 44100.0, 48000.0, 96000.0 }) {
                OfflineGraphAudioRequest request;
                request.sampleRate = rate;
                request.blockSize = 256;
                request.sampleCount = (size_t) (rate * 0.4);
                request.events = { { 0, MidiMessage::noteOn(1, 74, (uint8) 100) } };
                const auto dry = OfflineGraphAudioRenderer::render(dryCompiled.plan, 1, request);
                const auto wet = OfflineGraphAudioRenderer::render(wetCompiled.plan, 1, request);
                REQUIRE(dry.succeeded);
                REQUIRE(wet.succeeded);
                const double inputAmplitude = steadyToneAmplitude(dry.channels[0], rate, frequency);
                REQUIRE(inputAmplitude > 0.01);
                const double outputAmplitude = steadyToneAmplitude(wet.channels[0], rate, frequency);
                REQUIRE(outputAmplitude / inputAmplitude == Catch::Approx(expectedGain).epsilon(0.04));
            }
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
