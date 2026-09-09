#include <Audio/CycleDsp/SpectralStageCapture.h>

#include <algorithm>
#include <memory>

namespace CycleDsp {

using namespace juce;

namespace {

String primaryPayloadName(SpectralStage stage) {
    if (stage == SpectralStage::TimeRaster) {
        return "time-raster";
    }
    if (stage == SpectralStage::MagnitudeRaster) {
        return "magnitude-raster";
    }
    if (stage == SpectralStage::MagnitudeOperand) {
        return "magnitude-operand";
    }
    return stage == SpectralStage::ForwardFft
                    || stage == SpectralStage::PostLayerSpectrum
            ? "magnitude"
            : "samples";
}

String secondaryPayloadName(SpectralStage stage) {
    if (stage == SpectralStage::TimeRaster
            || stage == SpectralStage::MagnitudeRaster) {
        return "morph-position";
    }
    return stage == SpectralStage::ForwardFft
                    || stage == SpectralStage::PostLayerSpectrum
            ? "phase"
            : String {};
}

}

bool SpectralStageCaptureRecorder::prepare(
        int maximumValueCount,
        size_t targetFrameIndex) {
    if (maximumValueCount <= 0) {
        return false;
    }

    maximumValues = maximumValueCount;
    targetFrame = targetFrameIndex;
    payloadMemory.resize(
            stageCount * channelCount * 2 * maximumValues);
    reset();
    return true;
}

void SpectralStageCaptureRecorder::reset() {
    for (int stage = 0; stage < stageCount; ++stage) {
        for (int channel = 0; channel < channelCount; ++channel) {
            const int recordIndex = stage * channelCount + channel;
            const int payloadOffset = recordIndex * 2 * maximumValues;
            auto& captured = records[(size_t) recordIndex];
            captured = {};
            captured.stage = (SpectralStage) stage;
            captured.channel = channel;
            captured.primary = {
                    payloadMemory.get() + payloadOffset,
                    maximumValues
            };
            captured.secondary = {
                    payloadMemory.get() + payloadOffset + maximumValues,
                    maximumValues
            };
        }
    }
}

void SpectralStageCaptureRecorder::capture(
        const SpectralStageFrame& frame) noexcept {
    const int stage = stageIndex(frame.stage);
    if (frame.frameIndex != targetFrame
            || stage < 0
            || frame.channel < 0
            || frame.channel >= channelCount
            || frame.primary.empty()
            || frame.primary.size() > maximumValues
            || frame.secondary.size() > maximumValues) {
        return;
    }

    auto& captured = records[(size_t) (stage * channelCount + frame.channel)];
    if (captured.captured) {
        return;
    }

    frame.primary.copyTo(captured.primary.withSize(frame.primary.size()));
    if (!frame.secondary.empty()) {
        frame.secondary.copyTo(captured.secondary.withSize(frame.secondary.size()));
    }
    captured.frameIndex = frame.frameIndex;
    captured.frontier = frame.frontier;
    captured.midiNote = frame.midiNote;
    captured.primary = captured.primary.withSize(frame.primary.size());
    captured.secondary = captured.secondary.withSize(frame.secondary.size());
    captured.captured = true;
}

const CapturedSpectralStage* SpectralStageCaptureRecorder::record(
        SpectralStage stage,
        int channel) const {
    const int index = stageIndex(stage);
    if (index < 0 || channel < 0 || channel >= channelCount) {
        return nullptr;
    }
    const auto& captured = records[(size_t) (index * channelCount + channel)];
    return captured.captured ? &captured : nullptr;
}

bool SpectralStageCaptureRecorder::write(
        const File& manifest,
        String& error) const {
    if (manifest == File()) {
        error = "Spectral stage manifest path is empty";
        return false;
    }
    if (!manifest.getParentDirectory().createDirectory()) {
        error = "Could not create spectral stage capture directory";
        return false;
    }

    Array<var> encodedRecords;
    for (const auto& captured : records) {
        if (!captured.captured) {
            continue;
        }
        const String stem = manifest.getFileNameWithoutExtension()
                + "-" + spectralStageName(captured.stage)
                + "-frame-" + String((int64) captured.frameIndex)
                + "-channel-" + String(captured.channel);
        const File payload = manifest.getSiblingFile(stem + ".f32le");
        std::unique_ptr<FileOutputStream> stream(payload.createOutputStream());
        if (stream == nullptr || !stream->openedOk()
                || !stream->setPosition(0) || stream->truncate().failed()
                || !stream->write(
                        captured.primary.get(),
                        (size_t) captured.primary.size() * sizeof(float))
                || (!captured.secondary.empty()
                        && !stream->write(
                                captured.secondary.get(),
                                (size_t) captured.secondary.size() * sizeof(float)))) {
            error = "Could not write spectral stage payload: "
                    + payload.getFullPathName();
            return false;
        }
        stream.reset();

        var encoded = new DynamicObject();
        auto* object = encoded.getDynamicObject();
        object->setProperty("stage", spectralStageName(captured.stage));
        object->setProperty("frameIndex", (int64) captured.frameIndex);
        object->setProperty("frontier", (int64) captured.frontier);
        object->setProperty("midiNote", captured.midiNote);
        object->setProperty("channel", captured.channel);
        object->setProperty("primary", primaryPayloadName(captured.stage));
        object->setProperty("primaryValueCount", captured.primary.size());
        object->setProperty("secondary", secondaryPayloadName(captured.stage));
        object->setProperty("secondaryValueCount", captured.secondary.size());
        object->setProperty("rawPath", payload.getFullPathName());
        object->setProperty("sha256", SHA256(payload).toHexString());
        encodedRecords.add(encoded);
    }

    var root = new DynamicObject();
    auto* object = root.getDynamicObject();
    object->setProperty("schema", "cycle-spectral-stage-capture.v1");
    object->setProperty("targetFrameIndex", (int64) targetFrame);
    object->setProperty("records", encodedRecords);
    if (!manifest.replaceWithText(JSON::toString(root, true) + "\n")) {
        error = "Could not write spectral stage manifest: "
                + manifest.getFullPathName();
        return false;
    }
    return true;
}

int SpectralStageCaptureRecorder::stageIndex(SpectralStage stage) {
    const int index = (int) stage;
    return index >= 0 && index < stageCount ? index : -1;
}

String spectralStageName(SpectralStage stage) {
    switch (stage) {
        case SpectralStage::TimeRaster:
            return "time-raster";
        case SpectralStage::TimeFrame:
            return "time-frame";
        case SpectralStage::ForwardFft:
            return "forward-fft";
        case SpectralStage::MagnitudeRaster:
            return "magnitude-raster";
        case SpectralStage::MagnitudeOperand:
            return "magnitude-operand";
        case SpectralStage::PostLayerSpectrum:
            return "post-layer-spectrum";
        case SpectralStage::ReconstructedFrame:
            return "reconstructed-frame";
        case SpectralStage::PitchClockedCycle:
            return "pitch-clocked-cycle";
    }
    return "unknown";
}

}
