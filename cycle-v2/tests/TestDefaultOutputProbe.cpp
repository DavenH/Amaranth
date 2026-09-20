#include <catch2/catch_test_macros.hpp>

#include "Graph/DefaultOutputProbeResolver.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "UI/PresetPreviewGenerator.h"

using namespace CycleV2;
using namespace juce;

namespace {

NodeGraph chain(std::initializer_list<std::pair<NodeKind, String>> nodes) {
    NodeGraph graph;
    String previousId;
    String previousPort;
    for (const auto& [kind, id] : nodes) {
        Node node = GraphNodeFactory().createNode(kind, id, {});
        graph.addNode(std::move(node));
        if (previousId.isNotEmpty()) {
            graph.addEdge({ previousId, previousPort, id, "in", PortDomain::TimeSignal, false });
        }
        previousId = id;
        previousPort = "out";
    }
    return graph;
}

}

TEST_CASE("Default output probe excludes the wet and EQ suffix",
        "[cycle-v2][preset][preview][routing]") {
    const NodeGraph graph = chain({
            { NodeKind::GlobalInput, "input" },
            { NodeKind::Waveshaper, "shape" },
            { NodeKind::ImpulseResponse, "ir" },
            { NodeKind::Equalizer, "eq" },
            { NodeKind::Reverb, "verb" },
            { NodeKind::Delay, "delay" },
            { NodeKind::Output, "output" }
    });

    const auto address = DefaultOutputProbeResolver().resolve(graph);

    REQUIRE(address.has_value());
    REQUIRE(address->sourceNodeId == "ir");
    REQUIRE(address->sourcePortId == "out");
}

TEST_CASE("Default output probe retains a meaningful dry effect at the boundary",
        "[cycle-v2][preset][preview][routing]") {
    const NodeGraph graph = chain({
            { NodeKind::GlobalInput, "input" },
            { NodeKind::Waveshaper, "shape" },
            { NodeKind::Output, "output" }
    });

    const auto address = DefaultOutputProbeResolver().resolve(graph);

    REQUIRE(address.has_value());
    REQUIRE(address->sourceNodeId == "shape");
}

TEST_CASE("Default output probe handles direct and individually excluded boundaries",
        "[cycle-v2][preset][preview][routing]") {
    const NodeGraph direct = chain({
            { NodeKind::GlobalInput, "input" },
            { NodeKind::Output, "output" }
    });
    REQUIRE(DefaultOutputProbeResolver().resolve(direct)->sourceNodeId == "input");

    for (const auto& [kind, id] : {
            std::pair { NodeKind::Equalizer, String("eq") },
            std::pair { NodeKind::Reverb, String("verb") },
            std::pair { NodeKind::Delay, String("delay") }
    }) {
        const NodeGraph graph = chain({
                { NodeKind::GlobalInput, "input" },
                { NodeKind::Waveshaper, "shape" },
                { kind, id },
                { NodeKind::Output, "output" }
        });
        const auto address = DefaultOutputProbeResolver().resolve(graph);
        REQUIRE(address.has_value());
        REQUIRE(address->sourceNodeId == "shape");
    }
}

TEST_CASE("Default output probe rejects ambiguous and disconnected boundaries",
        "[cycle-v2][preset][preview][routing]") {
    NodeGraph disconnected;
    disconnected.addNode(GraphNodeFactory().createNode(NodeKind::Output, "output", {}));
    REQUIRE_FALSE(DefaultOutputProbeResolver().resolve(disconnected).has_value());

    NodeGraph ambiguous = chain({
            { NodeKind::GlobalInput, "input" },
            { NodeKind::Output, "output" }
    });
    ambiguous.addNode(GraphNodeFactory().createNode(NodeKind::GlobalInput, "second", {}));
    ambiguous.addEdge({ "second", "out", "output", "in", PortDomain::TimeSignal, false });
    REQUIRE_FALSE(DefaultOutputProbeResolver().resolve(ambiguous).has_value());
}

TEST_CASE("Compiled presets publish an implicit default output grid",
        "[cycle-v2][preset][preview][runtime]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content/presets/acidic-2.cyclegraph");
    const NodeGraph graph = GraphSerializer().fromJsonString(preset.loadFileAsString());
    const GraphCompileResult compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    REQUIRE(compiled.plan.defaultOutputProbe.has_value());

    AudioExecutionSpec spec;
    spec.maximumFrameCount = 512;
    spec.traversalColumnCount = 64;
    GraphAudioExecutor executor;
    executor.prepareExecution(compiled.plan, spec);
    const GraphAudioResult audio = executor.process(
            graph, compiled.plan, 512, {}, {}, spec.traversalColumnCount);
    const GraphPreviewResult preview = GraphPreviewExecutor().render(
            compiled.plan, audio, graph.getSignalProbes(), 64);

    REQUIRE(preview.defaultOutput.has_value());
    REQUIRE(preview.defaultOutput->connected);
    REQUIRE(preview.defaultOutput->gridColumns == 64);
    REQUIRE(preview.defaultOutput->gridRows == 512);
    REQUIRE(preview.defaultOutput->domain == PortDomain::TimeSignal);
    REQUIRE_FALSE(preview.defaultOutput->values.empty());
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Preset preview generator produces normalized time and spectral JPEGs",
        "[cycle-v2][preset][preview][image]") {
    GraphPreviewResult::SignalProbePreview time;
    time.connected = true;
    time.domain = PortDomain::TimeSignal;
    time.gridColumns = 4;
    time.gridRows = 8;
    time.values = {
            0.f, 0.1f, 0.2f, 0.1f, 0.f, -0.1f, -0.2f, -0.1f,
            0.f, 0.2f, 0.4f, 0.2f, 0.f, -0.2f, -0.4f, -0.2f,
            0.f, 0.3f, 0.6f, 0.3f, 0.f, -0.3f, -0.6f, -0.3f,
            0.f, 0.4f, 0.8f, 0.4f, 0.f, -0.4f, -0.8f, -0.4f
    };

    const auto spectral = PresetPreviewGenerator::forView(
            time, PresetPreviewView::Spectrum);
    REQUIRE(spectral.connected);
    REQUIRE(spectral.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(spectral.gridColumns == time.gridColumns);
    REQUIRE(spectral.gridRows == 5);
    REQUIRE(spectral.values != time.values);

    const PresetPreviewImage timeJpeg = PresetPreviewGenerator::encodeJpeg(
            time, PresetPreviewView::Time, 160, 90);
    const PresetPreviewImage spectrumJpeg = PresetPreviewGenerator::encodeJpeg(
            time, PresetPreviewView::Spectrum, 160, 90);
    REQUIRE(timeJpeg.isValid());
    REQUIRE(spectrumJpeg.isValid());
    REQUIRE(timeJpeg.jpegData != spectrumJpeg.jpegData);
    REQUIRE(ImageFileFormat::loadFrom(
            spectrumJpeg.jpegData.getData(),
            spectrumJpeg.jpegData.getSize()).getBounds() == Rectangle<int>(0, 0, 160, 90));
}
