#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Algo/FFT.h>
#include <Audio/CycleDsp/OscillatorLaneCore.h>
#include <Util/Arithmetic.h>

#include "Graph/GraphCompiler.h"
#include "Graph/GraphSerializer.h"
#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/SpectralOscillatorFrameRenderer.h"

#include <cmath>
#include <vector>

using namespace CycleV2;

namespace {

constexpr double sampleRate = 44100.0;
constexpr int blockSize = 256;
constexpr int renderedSamples = 32768;
constexpr int startupSamples = 1024;

std::vector<float> renderNote(
        const GraphExecutionPlan& plan,
        int midiNote) {
    AudioExecutionSpec spec;
    spec.maximumFrameCount = blockSize;
    spec.sampleRate = sampleRate;
    GraphAudioExecutor executor;
    executor.prepareExecution(plan, spec);

    AudioVoiceContext voice;
    voice.controls.noteNumber = midiNote;
    voice.controls.velocity = 1.f;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    std::vector<float> result;
    result.reserve(renderedSamples);
    for (int start = 0; start < renderedSamples; start += blockSize) {
        const auto output = executor.processRealtime(
                plan,
                blockSize,
                {},
                voice);
        if (!output.isValid()) {
            FAIL("Realtime spectral render did not produce an output block");
        }
        result.insert(
                result.end(),
                output.payload->block.samples.begin(),
                output.payload->block.samples.end());
        voice.events.clear();
    }
    return result;
}

float amplitudeAt(
        const std::vector<float>& samples,
        double frequency) {
    const double radians = 2.0 * MathConstants<double>::pi * frequency / sampleRate;
    const double rotationReal = std::cos(radians);
    const double rotationImaginary = -std::sin(radians);
    double oscillatorReal = 1.0;
    double oscillatorImaginary = 0.0;
    double projectionReal = 0.0;
    double projectionImaginary = 0.0;
    for (int index = startupSamples; index < (int) samples.size(); ++index) {
        projectionReal += samples[(size_t) index] * oscillatorReal;
        projectionImaginary += samples[(size_t) index] * oscillatorImaginary;
        const double nextReal = oscillatorReal * rotationReal
                - oscillatorImaginary * rotationImaginary;
        oscillatorImaginary = oscillatorReal * rotationImaginary
                + oscillatorImaginary * rotationReal;
        oscillatorReal = nextReal;
    }
    const double count = (double) samples.size() - startupSamples;
    return (float) (2.0 * std::sqrt(
            projectionReal * projectionReal
            + projectionImaginary * projectionImaginary) / count);
}

std::vector<float> fixedFrameMagnitudes(
        const GraphExecutionPlan& plan,
        int midiNote) {
    REQUIRE(plan.oscillatorRegions.size() == 1);
    const int frameSize = Arithmetic::getNextPow2((float) (
            1.0 / CycleDsp::OscillatorLaneCore::angleDelta(
                    midiNote,
                    0.f,
                    sampleRate)));
    SpectralOscillatorFrameRenderer renderer;
    REQUIRE(renderer.prepare(plan, plan.oscillatorRegions.front(), 16384));
    std::vector<float> frame((size_t) frameSize);
    std::vector<float> right((size_t) frameSize);
    REQUIRE(renderer.renderFrame(
            frameSize,
            midiNote,
            { frame.data(), frameSize },
            { right.data(), frameSize }));

    Transform transform;
    transform.allocate(frameSize, Transform::DivFwdByN, true);
    transform.forward({ frame.data(), frameSize });
    std::vector<float> magnitude((size_t) frameSize / 2 + 1);
    std::vector<float> phase(magnitude.size());
    transform.copyFullPolarSpectrumTo(
            { magnitude.data(), (int) magnitude.size() },
            { phase.data(), (int) phase.size() });
    return magnitude;
}

}

TEST_CASE("Spectral reference content remains harmonic after realtime reconstruction",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][audio-regression]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("spectral-reference.cyclegraph");
    REQUIRE(preset.existsAsFile());
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    const NodeGraph& graph = loaded.graph;
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    REQUIRE(compiled.plan.oscillatorRegions.size() == 1);
    REQUIRE(compiled.plan.oscillatorRegions.front().strategy
            == OscillatorExecutionStrategy::SharedSpectralFrame);

    for (const int midiNote : { 36, 48, 60, 72 }) {
        DYNAMIC_SECTION("MIDI note " << midiNote) {
            const auto expected = fixedFrameMagnitudes(compiled.plan, midiNote);
            const auto audio = renderNote(compiled.plan, midiNote);
            const double fundamentalFrequency = MidiMessage::getMidiNoteInHertz(midiNote);
            const float actualFundamental = amplitudeAt(audio, fundamentalFrequency);

            INFO("fixed-frame fundamental: " << expected[1]);
            INFO("rendered fundamental: " << actualFundamental);
            REQUIRE(expected[0] < 1.0e-5f);
            REQUIRE(expected[1] > 0.22f);
            REQUIRE(actualFundamental > 0.1f);

            for (int harmonic = 2; harmonic <= 4; ++harmonic) {
                const float expectedRatio = expected[(size_t) harmonic] / expected[1];
                const float actualRatio = amplitudeAt(
                        audio,
                        fundamentalFrequency * harmonic) / actualFundamental;
                INFO("harmonic " << harmonic << " expected ratio: " << expectedRatio);
                INFO("harmonic " << harmonic << " actual ratio: " << actualRatio);
                REQUIRE(actualRatio == Catch::Approx(expectedRatio).margin(0.06f));
            }

            const float betweenHarmonics = amplitudeAt(audio, fundamentalFrequency * 1.5);
            INFO("between-harmonic ratio: " << betweenHarmonics / actualFundamental);
            REQUIRE(betweenHarmonics / actualFundamental < 0.03f);
        }
    }
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
