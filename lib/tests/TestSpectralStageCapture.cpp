#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/SpectralStageCapture.h>

#include <array>
#include <cstring>

using namespace CycleDsp;

TEST_CASE("Spectral stage recorder captures one selected frame exactly") {
    SpectralStageCaptureRecorder recorder;
    REQUIRE(recorder.prepare(8, 2));

    std::array<float, 4> magnitude { 1.f, 2.f, 3.f, 4.f };
    std::array<float, 4> phase { 0.1f, 0.2f, 0.3f, 0.4f };
    recorder.capture({
            SpectralStage::ForwardFft,
            1,
            250,
            60,
            0,
            { magnitude.data(), (int) magnitude.size() },
            { phase.data(), (int) phase.size() }
    });
    REQUIRE(recorder.record(SpectralStage::ForwardFft, 0) == nullptr);

    recorder.capture({
            SpectralStage::ForwardFft,
            2,
            500,
            60,
            0,
            { magnitude.data(), (int) magnitude.size() },
            { phase.data(), (int) phase.size() }
    });
    const auto* captured = recorder.record(SpectralStage::ForwardFft, 0);
    REQUIRE(captured != nullptr);
    REQUIRE(captured->frameIndex == 2);
    REQUIRE(captured->frontier == 500);
    REQUIRE(captured->midiNote == 60);
    REQUIRE(captured->primary.size() == (int) magnitude.size());
    REQUIRE(captured->secondary.size() == (int) phase.size());
    REQUIRE(std::memcmp(
            captured->primary.get(),
            magnitude.data(),
            magnitude.size() * sizeof(float)) == 0);
    REQUIRE(std::memcmp(
            captured->secondary.get(),
            phase.data(),
            phase.size() * sizeof(float)) == 0);

    magnitude.fill(9.f);
    recorder.capture({
            SpectralStage::ForwardFft,
            2,
            500,
            60,
            0,
            { magnitude.data(), (int) magnitude.size() },
            { phase.data(), (int) phase.size() }
    });
    REQUIRE(captured->primary[0] == 1.f);
}

TEST_CASE("Spectral stage recorder writes hashed raw payload metadata") {
    SpectralStageCaptureRecorder recorder;
    REQUIRE(recorder.prepare(8, 0));
    std::array<float, 3> samples { -1.f, 0.25f, 0.75f };
    recorder.capture({
            SpectralStage::ReconstructedFrame,
            0,
            125,
            48,
            1,
            { samples.data(), (int) samples.size() },
            {}
    });

    juce::TemporaryFile temporary(".json");
    juce::String error;
    REQUIRE(recorder.write(temporary.getFile(), error));
    const juce::var manifest = juce::JSON::parse(
            temporary.getFile().loadFileAsString());
    REQUIRE(manifest.getProperty("schema", {}).toString()
            == "cycle-spectral-stage-capture.v1");
    const auto* records = manifest.getProperty("records", {}).getArray();
    REQUIRE(records != nullptr);
    REQUIRE(records->size() == 1);
    const juce::var& encoded = records->getReference(0);
    REQUIRE((int) encoded.getProperty("primaryValueCount", {}) == 3);
    REQUIRE((int) encoded.getProperty("secondaryValueCount", {}) == 0);

    const juce::File raw(encoded.getProperty("rawPath", {}).toString());
    REQUIRE(raw.existsAsFile());
    REQUIRE(raw.getSize() == (juce::int64) (samples.size() * sizeof(float)));
    REQUIRE(juce::SHA256(raw).toHexString()
            == encoded.getProperty("sha256", {}).toString());
    REQUIRE(raw.deleteFile());
}

TEST_CASE("Spectral stage recorder selects a repeated stage occurrence") {
    SpectralStageCaptureRecorder recorder;
    REQUIRE(recorder.prepare(8, 2, 1));

    std::array<float, 2> first { 1.f, 2.f };
    std::array<float, 2> second { 3.f, 4.f };
    recorder.capture({
            SpectralStage::MagnitudeOperand,
            2,
            500,
            60,
            0,
            { first.data(), (int) first.size() },
            {}
    });
    REQUIRE(recorder.record(SpectralStage::MagnitudeOperand, 0) == nullptr);

    recorder.capture({
            SpectralStage::MagnitudeOperand,
            2,
            500,
            60,
            0,
            { second.data(), (int) second.size() },
            {}
    });
    const auto* captured = recorder.record(SpectralStage::MagnitudeOperand, 0);
    REQUIRE(captured != nullptr);
    REQUIRE(captured->primary[0] == 3.f);
    REQUIRE(captured->primary[1] == 4.f);
}

TEST_CASE("Pitch-clocked stage metadata identifies the composed source cycle") {
    SpectralStageCaptureRecorder recorder;
    REQUIRE(recorder.prepare(8, 0));
    std::array<float, 2> samples { 0.25f, 0.5f };
    std::array<float, 3> composed { -1.f, 0.f, 1.f };
    recorder.capture({
            SpectralStage::PitchClockedCycle,
            0,
            125,
            48,
            0,
            { samples.data(), (int) samples.size() },
            { composed.data(), (int) composed.size() }
    });

    juce::TemporaryFile temporary(".json");
    juce::String error;
    REQUIRE(recorder.write(temporary.getFile(), error));
    const juce::var manifest = juce::JSON::parse(
            temporary.getFile().loadFileAsString());
    const auto* records = manifest.getProperty("records", {}).getArray();
    REQUIRE(records != nullptr);
    REQUIRE(records->size() == 1);
    const juce::var& encoded = records->getReference(0);
    REQUIRE(encoded.getProperty("secondary", {}).toString()
            == "composed-cycle");
    REQUIRE((int) encoded.getProperty("secondaryValueCount", {}) == 3);

    const juce::File raw(encoded.getProperty("rawPath", {}).toString());
    REQUIRE(raw.deleteFile());
}
