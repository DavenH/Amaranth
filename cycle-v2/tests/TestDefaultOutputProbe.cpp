#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "Graph/DefaultOutputProbeResolver.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/GraphPresentationModel.h"
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
    REQUIRE(address->destNodeId == "eq");
    REQUIRE(address->destPortId == "in");
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

TEST_CASE("Selected Spy capture matches full diagnostics without downstream work",
        "[cycle-v2][preset][preview][detail][regression]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content/presets/baroque-flute.cyclegraph");
    const NodeGraph graph = GraphSerializer().fromJsonString(preset.loadFileAsString());
    const GraphCompileResult compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    const auto found = std::find_if(
            compiled.plan.signalProbes.begin(),
            compiled.plan.signalProbes.end(),
            [](const auto& probe) {
                return probe.probeId == "probe";
            });
    REQUIRE(found != compiled.plan.signalProbes.end());

    AudioVoiceContext voice;
    voice.controls.noteNumber = 48;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    GraphAudioExecutor fullExecutor;
    const auto fullAudio = fullExecutor.process(graph, compiled.plan, 512, {}, voice, 512);
    const auto fullPreview = GraphPreviewExecutor().render(
            compiled.plan, fullAudio, graph.getSignalProbes(), 512);
    REQUIRE(fullPreview.probes.size() == 1);
    REQUIRE(fullPreview.probes.front().connected);

    GraphAudioExecutor selectedExecutor;
    const auto selectedAudio = selectedExecutor.processProbe(
            compiled.plan, *found, 512, {}, voice, 512);
    const auto selectedPreview = GraphPreviewExecutor().captureProbe(
            compiled.plan, selectedAudio, *found);
    REQUIRE(selectedPreview.connected);
    REQUIRE(selectedPreview.gridColumns == fullPreview.probes.front().gridColumns);
    REQUIRE(selectedPreview.gridRows == fullPreview.probes.front().gridRows);
    REQUIRE(selectedPreview.values == fullPreview.probes.front().values);
    REQUIRE(selectedAudio.nodes.size() == 1);
    REQUIRE(selectedAudio.nodes.front().nodeId == "volumeMultiply");
    REQUIRE(selectedExecutor.diagnosticProcessCount("reverb") == 0);
    REQUIRE(selectedExecutor.preparationCount("reverb") == 0);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("A Spy after reverb includes the selected wet effect",
        "[cycle-v2][preset][preview][detail][regression]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content/presets/baroque-flute.cyclegraph");
    NodeGraph graph = GraphSerializer().fromJsonString(preset.loadFileAsString());
    graph.addSignalProbe({
            "wet", "reverb", "time", "output", "time", "Wet", 0.5f, 2
    });
    const GraphCompileResult compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    const auto& wet = compiled.plan.signalProbes.back();
    REQUIRE(wet.probeId == "wet");

    AudioVoiceContext voice;
    voice.controls.noteNumber = 48;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    GraphAudioExecutor fullExecutor;
    const auto fullAudio = fullExecutor.process(graph, compiled.plan, 512, {}, voice, 128);
    const auto fullPreview = GraphPreviewExecutor().render(
            compiled.plan, fullAudio, graph.getSignalProbes(), 512);
    GraphAudioExecutor selectedExecutor;
    const auto selectedAudio = selectedExecutor.processProbe(
            compiled.plan, wet, 512, {}, voice, 128);
    const auto selectedPreview = GraphPreviewExecutor().captureProbe(
            compiled.plan, selectedAudio, wet);

    REQUIRE(selectedPreview.connected);
    REQUIRE(selectedPreview.values == fullPreview.probes.back().values);
    REQUIRE(selectedExecutor.diagnosticProcessCount("reverb") == 1);
    REQUIRE(selectedExecutor.diagnosticProcessCount("output") == 0);
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

TEST_CASE("Expanded output spectrum retains the compact FFT resolution",
        "[cycle-v2][preset][preview][spectrum][detail][regression]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content/presets/acidic-2.cyclegraph");
    const NodeGraph graph = GraphSerializer().fromJsonString(preset.loadFileAsString());
    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(graph, 1));
    REQUIRE(presentation.previewResult().defaultOutputSpectrum.has_value());
    const auto& compact = *presentation.previewResult().defaultOutputSpectrum;
    REQUIRE(compact.gridColumns == 256);
    REQUIRE(compact.gridRows == 257);
    REQUIRE(compact.values.size() == compact.gridColumns * compact.gridRows);

    const size_t noteRows = GraphPreviewExecutor::periodRowsForMidiNote(
            presentation.previewMidiNote());
    const size_t sourceRows = PresetPreviewGenerator::sourceRowCountForView(
            noteRows,
            PresetPreviewView::Spectrum);
    const auto detailTime = presentation.captureProbePreview(
            graph,
            DefaultOutputProbeResolver::probeId,
            sourceRows,
            presentation.previewMidiNote());
    REQUIRE(noteRows < 512);
    REQUIRE(sourceRows == 512);
    REQUIRE(detailTime.has_value());
    AudioVoiceContext voice;
    voice.controls.noteNumber = presentation.previewMidiNote();
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    GraphAudioExecutor fullExecutor;
    const auto fullAudio = fullExecutor.process(
            graph, presentation.compileResult().plan, sourceRows, {}, voice, 512);
    const auto fullPreview = GraphPreviewExecutor().render(
            presentation.compileResult().plan,
            fullAudio,
            graph.getSignalProbes(),
            sourceRows);
    REQUIRE(fullPreview.defaultOutput.has_value());
    REQUIRE(detailTime->values == fullPreview.defaultOutput->values);
    const auto detail = PresetPreviewGenerator::forView(
            *detailTime,
            PresetPreviewView::Spectrum);
    REQUIRE(detail.gridColumns == 512);
    REQUIRE(detail.gridRows == compact.gridRows);
    REQUIRE(detail.values.size() == detail.gridColumns * detail.gridRows);
    const auto captureStages = presentation.performanceMetrics()
            .getDynamicObject()->getProperty("stages");
    for (const char* stage : { "expandedProbeExecution",
                 "expandedProbeExtraction", "expandedProbeTotal" }) {
        const auto distribution = captureStages.getDynamicObject()->getProperty(stage);
        REQUIRE((int64) distribution.getDynamicObject()->getProperty("count") == 1);
    }

    double meanDifference {};
    for (size_t column = 0; column < compact.gridColumns; ++column) {
        const size_t detailColumn = (size_t) std::round(
                (double) column * (double) (detail.gridColumns - 1)
                / (double) (compact.gridColumns - 1));
        for (size_t row = 0; row < compact.gridRows; ++row) {
            meanDifference += std::abs(
                    compact.values[column * compact.gridRows + row]
                    - detail.values[detailColumn * detail.gridRows + row]);
        }
    }
    meanDifference /= (double) compact.values.size();
    REQUIRE(meanDifference < 0.02);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
