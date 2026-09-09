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
