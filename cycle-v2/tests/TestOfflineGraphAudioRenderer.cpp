#include <algorithm>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Array/Buffer.h>
#include <Audio/CycleDsp/SpectralStageCapture.h>
#include <Util/LogRegionMapping.h>

#include "App/OfflineAudioCaptureAutomation.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphSerializer.h"
#include "Runtime/OfflineGraphAudioRenderer.h"

using namespace CycleV2;
using namespace juce;

namespace {

GraphExecutionPlan spectralReferencePlan() {
#if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("spectral-reference.cyclegraph");
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    const auto compiled = GraphCompiler().compile(loaded.graph);
    REQUIRE(compiled.succeeded());
    return compiled.plan;
#else
    return {};
#endif
}

GraphExecutionPlan subbassParityPlan() {
#if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("subbass-parity.cyclegraph");
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    String issueMessages;
    for (const auto& issue : loaded.issues) {
        issueMessages += issue.message + "\n";
    }
    INFO(issueMessages);
    REQUIRE(loaded.succeeded());
    const auto compiled = GraphCompiler().compile(loaded.graph);
    REQUIRE(compiled.succeeded());
    return compiled.plan;
#else
    return {};
#endif
}

GraphExecutionPlan filterSawPlan() {
#if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("filter-saw.cyclegraph");
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    const auto compiled = GraphCompiler().compile(loaded.graph);
    REQUIRE(compiled.succeeded());
    return compiled.plan;
#else
    return {};
#endif
}

OfflineGraphAudioRequest renderRequest(int blockSize, int midiNote = 72) {
    OfflineGraphAudioRequest request;
    request.sampleRate = 48000.0;
    request.blockSize = blockSize;
    request.channelCount = 2;
    request.sampleCount = 4096;
    request.events = {
            { 37, MidiMessage::noteOn(1, midiNote, (uint8) 96) },
            { 3073, MidiMessage::noteOff(1, midiNote) }
    };
    return request;
}

}

TEST_CASE("Offline graph renderer follows the realtime MIDI path across blocks",
        "[cycle-v2][runtime][offline-audio][spectral]") {
#if defined(CYCLE_V2_SOURCE_DIR)
    const auto plan = spectralReferencePlan();
    const auto regular = OfflineGraphAudioRenderer::render(
            plan,
            7,
            renderRequest(256));
    const auto partitioned = OfflineGraphAudioRenderer::render(
            plan,
            7,
            renderRequest(173));

    REQUIRE(regular.succeeded);
    REQUIRE(partitioned.succeeded);
    REQUIRE(regular.channels[0].size() == 4096);
    REQUIRE(regular.channels[1].size() == 4096);
    REQUIRE(std::all_of(
            regular.channels[0].begin(),
            regular.channels[0].begin() + 37,
            [](float sample) {
                return sample == 0.f;
            }));
    REQUIRE(Buffer<float>(
            const_cast<float*>(regular.channels[0].data()),
            (int) regular.channels[0].size()).normL2() > 0.01f);
    REQUIRE(Buffer<float>(
            const_cast<float*>(regular.channels[0].data()),
            (int) regular.channels[0].size()).normDiffL2({
                    const_cast<float*>(partitioned.channels[0].data()),
                    (int) partitioned.channels[0].size()
            }) < 1.0e-6f);
    REQUIRE(Buffer<float>(
            const_cast<float*>(regular.channels[1].data()),
            (int) regular.channels[1].size()).normDiffL2({
                    const_cast<float*>(partitioned.channels[1].data()),
                    (int) partitioned.channels[1].size()
            }) < 1.0e-6f);
#else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
#endif
}

TEST_CASE("Offline graph renderer applies the requested output gain",
        "[cycle-v2][runtime][offline-audio][output-gain]") {
#if defined(CYCLE_V2_SOURCE_DIR)
    auto fullGainRequest = renderRequest(256);
    fullGainRequest.outputGain = 0.125f;
    auto halfGainRequest = fullGainRequest;
    halfGainRequest.outputGain = 0.0625f;

    const auto plan = spectralReferencePlan();
    const auto fullGain = OfflineGraphAudioRenderer::render(
            plan,
            7,
            fullGainRequest);
    const auto halfGain = OfflineGraphAudioRenderer::render(
            plan,
            7,
            halfGainRequest);

    REQUIRE(fullGain.succeeded);
    REQUIRE(halfGain.succeeded);
    std::vector<float> expected = fullGain.channels[0];
    Buffer<float>(expected.data(), (int) expected.size()).mul(0.5f);
    REQUIRE(Buffer<float>(
            const_cast<float*>(halfGain.channels[0].data()),
            (int) halfGain.channels[0].size()).normDiffL2({
                    expected.data(),
                    (int) expected.size()
            }) < 1.0e-7f);
#else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
#endif
}

TEST_CASE("Legacy-rate offline rendering is deterministic and distinct from native rate",
        "[cycle-v2][runtime][offline-audio][internal-rate][parity]") {
#if defined(CYCLE_V2_SOURCE_DIR)
    auto legacyRequest = renderRequest(256, 48);
    legacyRequest.ratePolicy = OfflineGraphAudioRatePolicy::LegacyInternal44100;
    auto nativeRequest = legacyRequest;
    nativeRequest.ratePolicy = OfflineGraphAudioRatePolicy::Native;

    const auto plan = filterSawPlan();
    const auto legacy = OfflineGraphAudioRenderer::render(
            plan,
            12,
            legacyRequest);
    const auto repeated = OfflineGraphAudioRenderer::render(
            plan,
            12,
            legacyRequest);
    const auto native = OfflineGraphAudioRenderer::render(
            plan,
            12,
            nativeRequest);

    REQUIRE(legacy.succeeded);
    REQUIRE(repeated.succeeded);
    REQUIRE(native.succeeded);
    REQUIRE(Buffer<float>(
            const_cast<float*>(legacy.channels[0].data()),
            (int) legacy.channels[0].size()).normDiffL2({
                    const_cast<float*>(repeated.channels[0].data()),
                    (int) repeated.channels[0].size()
            }) < 1.0e-6f);
    REQUIRE(Buffer<float>(
            const_cast<float*>(legacy.channels[0].data()),
            (int) legacy.channels[0].size()).normDiffL2({
                    const_cast<float*>(native.channels[0].data()),
                    (int) native.channels[0].size()
            }) > 0.01f);
#else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
#endif
}

TEST_CASE("Offline spectral capture records equivalent harmonic boundaries",
        "[cycle-v2][runtime][offline-audio][spectral][parity]") {
#if defined(CYCLE_V2_SOURCE_DIR)
    CycleDsp::SpectralStageCaptureRecorder recorder;
    REQUIRE(recorder.prepare(4096, 32));
    auto request = renderRequest(256, 48);
    request.sampleCount = 12000;
    request.events = {
            { 0, MidiMessage::noteOn(1, 48, (uint8) 96) },
            { 11999, MidiMessage::noteOff(1, 48) }
    };
    request.voiceDurationSeconds = 0.47689543975042176f;
    request.controlNoteOffset = 12;
    request.ratePolicy = OfflineGraphAudioRatePolicy::LegacyInternal44100;
    request.spectralStageCapture = &recorder;
    const auto plan = filterSawPlan();
    const auto magnitudeStep = std::find_if(
            plan.steps.begin(),
            plan.steps.end(),
            [](const GraphExecutionStep& step) {
                return step.nodeId == "magnitudeLayer1";
            });
    REQUIRE(magnitudeStep != plan.steps.end());
    REQUIRE(std::count_if(
            magnitudeStep->inputs.begin(),
            magnitudeStep->inputs.end(),
            [](const GraphStepInput& input) {
                return input.destPortId == "yellow"
                        || input.destPortId == "red"
                        || input.destPortId == "blue";
            }) == 3);
    REQUIRE(std::all_of(
            magnitudeStep->inputs.begin(),
            magnitudeStep->inputs.end(),
            [](const GraphStepInput& input) {
                return input.destPortId == "context"
                        || input.sourceBufferIndex >= 0;
            }));

    const auto result = OfflineGraphAudioRenderer::render(
            plan,
            12,
            request);

    REQUIRE(result.succeeded);
    const auto* timeRaster = recorder.record(
            CycleDsp::SpectralStage::TimeRaster,
            0);
    const auto* time = recorder.record(
            CycleDsp::SpectralStage::TimeFrame,
            0);
    const auto* forward = recorder.record(
            CycleDsp::SpectralStage::ForwardFft,
            0);
    const auto* magnitudeRaster = recorder.record(
            CycleDsp::SpectralStage::MagnitudeRaster,
            0);
    const auto* magnitudeOperand = recorder.record(
            CycleDsp::SpectralStage::MagnitudeOperand,
            0);
    const auto* postLayer = recorder.record(
            CycleDsp::SpectralStage::PostLayerSpectrum,
            0);
    const auto* reconstructed = recorder.record(
            CycleDsp::SpectralStage::ReconstructedFrame,
            0);
    const auto* pitchClocked = recorder.record(
            CycleDsp::SpectralStage::PitchClockedCycle,
            0);
    REQUIRE(time != nullptr);
    REQUIRE(timeRaster != nullptr);
    REQUIRE(timeRaster->secondary.size() == 3);
    REQUIRE(timeRaster->secondary[0] == Catch::Approx(0.7148094f));
    REQUIRE(timeRaster->secondary[1] == Catch::Approx(40.f / 107.f));
    REQUIRE(forward != nullptr);
    REQUIRE(magnitudeRaster != nullptr);
    REQUIRE(magnitudeOperand != nullptr);
    REQUIRE(postLayer != nullptr);
    REQUIRE(reconstructed != nullptr);
    REQUIRE(pitchClocked != nullptr);
    REQUIRE(time->primary.size() == reconstructed->primary.size());
    REQUIRE(forward->primary.size()
            == LogRegionMapping(
                    48 + LogRegionMapping::legacyMidiNoteBias).regionSize());
    REQUIRE(forward->secondary.size() == forward->primary.size());
    REQUIRE(magnitudeRaster->primary.size() == forward->primary.size());
    REQUIRE(magnitudeRaster->secondary.size() == 3);
    REQUIRE(magnitudeRaster->secondary[1]
            == Catch::Approx(40.f / 107.f));
    REQUIRE(magnitudeRaster->secondary[2]
            == Catch::Approx(1.f - 96.f / 127.f));
    REQUIRE(magnitudeOperand->primary.size() == forward->primary.size());
    REQUIRE(postLayer->primary.size() == forward->primary.size());
    REQUIRE(postLayer->secondary.size() == forward->secondary.size());
    REQUIRE(time->frontier == forward->frontier);
    REQUIRE(forward->frontier == magnitudeRaster->frontier);
    REQUIRE(magnitudeRaster->frontier == magnitudeOperand->frontier);
    REQUIRE(magnitudeOperand->frontier == postLayer->frontier);
    REQUIRE(postLayer->frontier == reconstructed->frontier);
    REQUIRE_FALSE(pitchClocked->primary.empty());
#else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
#endif
}

TEST_CASE("Offline graph renderer rejects invalid render contracts",
        "[cycle-v2][runtime][offline-audio]") {
    OfflineGraphAudioRequest request;
    request.sampleCount = 64;
    request.channelCount = 3;

    const auto result = OfflineGraphAudioRenderer::render({}, 1, request);

    REQUIRE_FALSE(result.succeeded);
    REQUIRE(result.error == "Channel count must be one or two");
}

TEST_CASE("Compiled Voice Context octave reaches the oscillator region",
        "[cycle-v2][runtime][offline-audio][voice-context]") {
#if defined(CYCLE_V2_SOURCE_DIR)
    auto shiftedPlan = spectralReferencePlan();
    REQUIRE(shiftedPlan.voiceContexts.size() == 1);
    shiftedPlan.voiceContexts.front().octave = -1;
    const auto shifted = OfflineGraphAudioRenderer::render(
            shiftedPlan,
            8,
            renderRequest(256, 72));
    const auto reference = OfflineGraphAudioRenderer::render(
            spectralReferencePlan(),
            8,
            renderRequest(256, 60));

    REQUIRE(shifted.succeeded);
    REQUIRE(reference.succeeded);
    REQUIRE(Buffer<float>(
            const_cast<float*>(shifted.channels[0].data()),
            (int) shifted.channels[0].size()).normDiffL2({
                    const_cast<float*>(reference.channels[0].data()),
                    (int) reference.channels[0].size()
            }) < 1.0e-6f);
#else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
#endif
}

TEST_CASE("Scheduled audio automation shares the Cycle capture contract",
        "[cycle-v2][automation][offline-audio]") {
#if defined(CYCLE_V2_SOURCE_DIR)
    var command = new DynamicObject();
    DynamicObject* object = command.getDynamicObject();
    object->setProperty("sampleRate", 48000.0);
    object->setProperty("blockSize", 173);
    object->setProperty("channels", 2);
    object->setProperty("durationMs", 100.0);
    object->setProperty("voiceDurationSeconds", 1.25);
    object->setProperty("controlNoteOffset", 12);
    object->setProperty("ratePolicy", "legacyInternal44100");

    Array<var> events;
    var noteOn = new DynamicObject();
    noteOn.getDynamicObject()->setProperty("type", "noteOn");
    noteOn.getDynamicObject()->setProperty("sample", 37);
    noteOn.getDynamicObject()->setProperty("note", 72);
    events.add(noteOn);
    object->setProperty("events", events);

    var data;
    String error;
    REQUIRE(OfflineAudioCaptureAutomation::isScheduledCapture(command));
    REQUIRE(OfflineAudioCaptureAutomation::capture(
            command,
            {},
            spectralReferencePlan(),
            9,
            data,
            error));
    REQUIRE((double) data.getProperty("sampleRate", 0.0) == 48000.0);
    REQUIRE((int) data.getProperty("channels", 0) == 2);
    REQUIRE((int64) data.getProperty("samples", 0) == 4800);
    REQUIRE((int) data.getProperty("events", 0) == 1);
    REQUIRE((double) data.getProperty("voiceDurationSeconds", 0.0) == 1.25);
    REQUIRE((int) data.getProperty("controlNoteOffset", 0) == 12);
    REQUIRE(data.getProperty("ratePolicy", {}).toString() == "legacyInternal44100");
    REQUIRE((double) data.getProperty("rms", 0.0) > 0.0);
#else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
#endif
}

TEST_CASE("Legacy diagnostic capture is not mistaken for scheduled capture",
        "[cycle-v2][automation][offline-audio]") {
    var command = new DynamicObject();
    command.getDynamicObject()->setProperty("frames", 1024);

    REQUIRE_FALSE(OfflineAudioCaptureAutomation::isScheduledCapture(command));
}

TEST_CASE("Strictly ported subbass fixture renders through the realtime path",
        "[cycle-v2][runtime][offline-audio][parity-fixture]") {
#if defined(CYCLE_V2_SOURCE_DIR)
    const auto plan = subbassParityPlan();
    REQUIRE(plan.voiceContexts.size() == 1);
    REQUIRE(plan.voiceContexts.front().octave == -2);
    const auto capture = OfflineGraphAudioRenderer::render(
            plan,
            11,
            renderRequest(256));

    REQUIRE(capture.succeeded);
    REQUIRE(Buffer<float>(
            const_cast<float*>(capture.channels[0].data()),
            (int) capture.channels[0].size()).normL2() > 0.01f);
#else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
#endif
}
