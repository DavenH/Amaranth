#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Algo/FFT.h>
#include <Algo/Resampling.h>
#include <Audio/CycleDsp/OscillatorLaneCore.h>
#include <Audio/CycleDsp/SpectralStageCapture.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegionMapping.h>

#include "Graph/GraphCompiler.h"
#include "Graph/GraphSerializer.h"
#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/SpectralOscillatorFrameRenderer.h"

using namespace CycleV2;

namespace {

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;
constexpr int renderedSamples = 33792;
constexpr int startupSamples = 1024;
constexpr int reconstructionLatencySamples = 0;
constexpr int hermitePhaseDelaySamples = 3;

struct FoldConsistency {
    float maximumNormalizedError {};
    float meanNormalizedError {};
};

GraphExecutionPlan loadPresetPlan(const String& name) {
#if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile(name + ".cyclegraph");
    REQUIRE(preset.existsAsFile());
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    const auto compiled = GraphCompiler().compile(loaded.graph);
    REQUIRE(compiled.succeeded());
    REQUIRE(compiled.plan.oscillatorRegions.size() == 1);
    REQUIRE(compiled.plan.oscillatorRegions.front().strategy
            == OscillatorExecutionStrategy::SharedSpectralFrame);
    return compiled.plan;
#else
    return {};
#endif
}

GraphExecutionPlan loadSpectralReferencePlan() {
    return loadPresetPlan("spectral-reference");
}

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

float pitchUnitForFrequency(int midiNote, double frequency) {
    const double midiPitch = 69.0 + 12.0 * std::log2(frequency / 440.0);
    const double semitoneOffset = midiPitch - (double) midiNote;
    REQUIRE(semitoneOffset > -12.0);
    REQUIRE(semitoneOffset < 12.0);
    return (float) (0.5 + semitoneOffset / 24.0);
}

std::vector<std::vector<float>> phaseFold(
        const std::vector<float>& samples,
        double angleDelta,
        int phaseBinCount,
        int discardedCycleCount,
        int retainedCycleCount) {
    CycleDsp::ChainedCycleState clock;
    int cycleStart = reconstructionLatencySamples;
    std::vector<std::vector<float>> rows;
    rows.reserve((size_t) retainedCycleCount);

    for (int cycle = 0; cycle < discardedCycleCount + retainedCycleCount; ++cycle) {
        CycleDsp::OscillatorLaneCore::advanceChainedCycle(clock, angleDelta);
        const int cycleSampleCount = clock.samplesThisCycle;
        REQUIRE(cycleSampleCount > 0);
        REQUIRE(cycleStart + cycleSampleCount < (int) samples.size());

        if (cycle >= discardedCycleCount) {
            std::vector<float> row((size_t) phaseBinCount);
            for (int bin = 0; bin < phaseBinCount; ++bin) {
                const double position = (double) bin * cycleSampleCount / phaseBinCount;
                const int lower = (int) position;
                const float fraction = (float) (position - lower);
                const float a = samples[(size_t) (cycleStart + lower)];
                const float b = samples[(size_t) (cycleStart + lower + 1)];
                row[(size_t) bin] = Resampling::lerp(a, b, fraction);
            }
            rows.push_back(std::move(row));
        }
        cycleStart += cycleSampleCount;
    }
    return rows;
}

float normalizedDifference(
        const std::vector<float>& reference,
        const std::vector<float>& row) {
    REQUIRE(row.size() == reference.size());
    double referenceEnergy = 0.0;
    double errorEnergy = 0.0;
    for (size_t sampleIndex = 0; sampleIndex < reference.size(); ++sampleIndex) {
        const double sample = reference[sampleIndex];
        const double difference = row[sampleIndex] - sample;
        referenceEnergy += sample * sample;
        errorEnergy += difference * difference;
    }
    REQUIRE(referenceEnergy > std::numeric_limits<double>::epsilon());
    return (float) std::sqrt(errorEnergy / referenceEnergy);
}

FoldConsistency measureFoldConsistency(const std::vector<std::vector<float>>& rows) {
    REQUIRE(rows.size() > 1);
    const auto& reference = rows.front();
    REQUIRE(!reference.empty());

    double totalError = 0.0;
    float maximumError = 0.f;
    for (size_t rowIndex = 1; rowIndex < rows.size(); ++rowIndex) {
        const float normalizedError = normalizedDifference(reference, rows[rowIndex]);
        maximumError = std::max(maximumError, normalizedError);
        totalError += normalizedError;
    }

    return {
            maximumError,
            (float) (totalError / (double) (rows.size() - 1))
    };
}

std::vector<float> renderFixedFrame(
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
    return frame;
}

std::vector<float> fixedFrameMagnitudes(
        const GraphExecutionPlan& plan,
        int midiNote) {
    auto frame = renderFixedFrame(plan, midiNote);

    Transform transform;
    transform.allocate((int) frame.size(), Transform::DivFwdByN, true);
    transform.forward({ frame.data(), (int) frame.size() });
    std::vector<float> magnitude(frame.size() / 2 + 1);
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
    const auto plan = loadSpectralReferencePlan();

    for (const int midiNote : { 36, 48, 60, 72 }) {
        DYNAMIC_SECTION("MIDI note " << midiNote) {
            const auto expected = fixedFrameMagnitudes(plan, midiNote);
            const auto audio = renderNote(plan, midiNote);
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

TEST_CASE("Prepared spectral reconstruction retains the final legacy harmonic",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][parity]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    constexpr int midiNote = 48;
    const auto plan = loadPresetPlan("filter-saw");
    const int frameSize = Arithmetic::getNextPow2((float) (
            1.0 / CycleDsp::OscillatorLaneCore::angleDelta(
                    midiNote,
                    0.f,
                    sampleRate)));
    const int activeHarmonicCount = LogRegionMapping(
            midiNote + LogRegionMapping::legacyMidiNoteBias).regionSize();

    SpectralOscillatorFrameRenderer renderer;
    REQUIRE(renderer.prepare(plan, plan.oscillatorRegions.front(), 16384));
    CycleDsp::SpectralStageCaptureRecorder recorder;
    REQUIRE(recorder.prepare(frameSize, 0));
    std::vector<SignalPayload> signals(plan.buffers.size());
    for (auto& signal : signals) {
        signal.domain = PortDomain::ControlSignal;
        signal.channelLayout = ChannelLayout::Mono;
        signal.block.samples.assign((size_t) frameSize, 0.05f);
    }
    AudioVoiceContext voice;
    voice.hasLifecycleSeed = true;
    voice.lifecycleSeed = 0x43594332u;
    voice.controls.normalizedVoiceTimeIncrement = 1.f / (48000.f * 0.47689545f);
    voice.spectralStageCapture = &recorder;
    PreparedOscillatorProcessContext context;
    context.voice = &voice;
    context.signalBuffers = signals.data();
    context.signalBufferCount = signals.size();
    context.blockFrameCount = (size_t) frameSize;
    context.timing.sampleRate = sampleRate;
    std::vector<float> frame((size_t) frameSize);
    std::vector<float> right((size_t) frameSize);

    renderer.applyLifecycleEvent({ NoteLifecycleType::NoteOn, 0, 0 });
    REQUIRE(renderer.renderFrame(
            frameSize,
            midiNote,
            context,
            0,
            0.0,
            1,
            { frame.data(), frameSize },
            { right.data(), frameSize }));
    const auto* spectrum = recorder.record(
            CycleDsp::SpectralStage::PostLayerSpectrum,
            0);
    REQUIRE(spectrum != nullptr);
    REQUIRE(spectrum->primary.size() == activeHarmonicCount);
    REQUIRE(spectrum->primary.back() > 1.0e-7f);

    Transform transform;
    transform.allocate(frameSize, Transform::DivFwdByN, true);
    transform.forward({ frame.data(), frameSize });
    std::vector<float> magnitudes((size_t) frameSize / 2 + 1);
    std::vector<float> phases(magnitudes.size());
    transform.copyFullPolarSpectrumTo(
            { magnitudes.data(), (int) magnitudes.size() },
            { phases.data(), (int) phases.size() });

    REQUIRE(magnitudes[(size_t) activeHarmonicCount]
            == Catch::Approx(spectrum->primary.back()).epsilon(0.02).margin(1.0e-9));
    REQUIRE(magnitudes[(size_t) activeHarmonicCount + 1] < 1.0e-8f);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Exact-period spectral reconstruction repeats one stable cyclogram row",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][cyclogram]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    constexpr int midiNote = 36;
    constexpr int samplesPerCycle = 1024;
    constexpr double frequency = sampleRate / samplesPerCycle;
    auto plan = loadSpectralReferencePlan();
    REQUIRE(plan.voiceContexts.size() == 1);
    plan.voiceContexts.front().pitchEnvelopeUnitValues = {
            pitchUnitForFrequency(midiNote, frequency)
    };

    const auto audio = renderNote(plan, midiNote);
    const auto rows = phaseFold(
            audio,
            1.0 / samplesPerCycle,
            samplesPerCycle,
            4,
            24);
    const auto consistency = measureFoldConsistency(rows);
    auto expectedFrame = renderFixedFrame(plan, midiNote);
    std::rotate(
            expectedFrame.begin(),
            expectedFrame.end() - hermitePhaseDelaySamples,
            expectedFrame.end());
    const float reconstructionError = normalizedDifference(expectedFrame, rows.front());

    INFO("maximum normalized cyclogram-row error: "
            << consistency.maximumNormalizedError);
    INFO("mean normalized cyclogram-row error: "
            << consistency.meanNormalizedError);
    INFO("normalized error against the rendered spectral frame: "
            << reconstructionError);
    REQUIRE(consistency.maximumNormalizedError < 1.0e-4f);
    REQUIRE(consistency.meanNormalizedError < 1.0e-5f);
    REQUIRE(reconstructionError < 1.0e-4f);

    const float fundamental = amplitudeAt(audio, frequency);
    INFO("exact-period fundamental amplitude: " << fundamental);
    REQUIRE(fundamental > 0.1f);
    REQUIRE(amplitudeAt(audio, frequency * 1.5) / fundamental < 0.02f);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Fractional-period MIDI spectral reconstruction remains phase-folded",
        "[cycle-v2][runtime][oscillator-region][spectral-frame][cyclogram]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    constexpr int midiNote = 72;
    constexpr int phaseBinCount = 512;
    const auto plan = loadSpectralReferencePlan();
    const auto audio = renderNote(plan, midiNote);
    const double angleDelta = CycleDsp::OscillatorLaneCore::angleDelta(
            midiNote,
            0.f,
            sampleRate);
    const auto rows = phaseFold(audio, angleDelta, phaseBinCount, 12, 64);
    const auto consistency = measureFoldConsistency(rows);

    INFO("MIDI 72 period in samples: " << 1.0 / angleDelta);
    INFO("maximum normalized phase-folded row error across cycle lengths: "
            << consistency.maximumNormalizedError);
    INFO("mean normalized phase-folded row error across cycle lengths: "
            << consistency.meanNormalizedError);
    REQUIRE(consistency.maximumNormalizedError < 0.05f);
    REQUIRE(consistency.meanNormalizedError < 0.025f);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
