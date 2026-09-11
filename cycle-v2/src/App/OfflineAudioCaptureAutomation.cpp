#include <cmath>

#include <Array/Buffer.h>
#include <Audio/CycleDsp/SpectralStageCapture.h>

#include "App/OfflineAudioCaptureAutomation.h"
#include "Runtime/OfflineGraphAudioRenderer.h"

namespace CycleV2 {

using namespace juce;

namespace {

var property(const var& value, const Identifier& name) {
    const DynamicObject* object = value.getDynamicObject();
    return object != nullptr ? object->getProperty(name) : var {};
}

double doubleProperty(
        const var& value,
        const Identifier& name,
        double fallback) {
    const var found = property(value, name);
    return found.isVoid() ? fallback : (double) found;
}

String stringProperty(
        const var& value,
        const Identifier& name,
        const String& fallback = {}) {
    const var found = property(value, name);
    return found.isVoid() ? fallback : found.toString();
}

size_t eventSample(
        const var& event,
        double sampleRate,
        size_t sampleCount) {
    const var sample = property(event, "sample");
    if (!sample.isVoid()) {
        return (size_t) jlimit<int64>(0, (int64) sampleCount, (int64) sample);
    }

    const double timeMs = doubleProperty(
            event,
            "timeMs",
            doubleProperty(event, "time", 0.0));
    return (size_t) jlimit<int64>(
            0,
            (int64) sampleCount,
            (int64) std::round(timeMs * sampleRate / 1000.0));
}

void addEvent(
        OfflineGraphAudioRequest& request,
        size_t sampleOffset,
        const MidiMessage& message) {
    request.events.push_back({
            sampleOffset,
            message,
            MidiEventSource::PerformanceKeyboard
    });
}

bool parseEvent(
        const var& event,
        OfflineGraphAudioRequest& request,
        String& error) {
    const String type = stringProperty(
            event,
            "type",
            stringProperty(event, "event", "noteOn"));
    const int channel = jlimit(1, 16, (int) doubleProperty(event, "channel", 1.0));
    const size_t sampleOffset = eventSample(event, request.sampleRate, request.sampleCount);

    if (type == "noteOn" || type == "note") {
        const int note = jlimit(0, 127, (int) doubleProperty(event, "note", 60.0));
        const float velocity = jlimit(0.f, 1.f, (float) doubleProperty(event, "velocity", 0.8));
        addEvent(request, sampleOffset, MidiMessage::noteOn(channel, note, velocity));
        return true;
    }
    if (type == "noteOff") {
        const int note = jlimit(0, 127, (int) doubleProperty(event, "note", 60.0));
        addEvent(request, sampleOffset, MidiMessage::noteOff(channel, note));
        return true;
    }
    if (type == "controller") {
        const int number = jlimit(
                0,
                127,
                (int) doubleProperty(event, "controller", doubleProperty(event, "number", 0.0)));
        const int controllerValue = jlimit(0, 127, (int) doubleProperty(event, "value", 0.0));
        addEvent(request, sampleOffset, MidiMessage::controllerEvent(channel, number, controllerValue));
        return true;
    }
    if (type == "channelPressure") {
        const int pressure = jlimit(0, 127, (int) doubleProperty(event, "value", 0.0));
        addEvent(request, sampleOffset, MidiMessage::channelPressureChange(channel, pressure));
        return true;
    }
    if (type == "allNotesOff") {
        addEvent(request, sampleOffset, MidiMessage::allNotesOff(channel));
        return true;
    }

    error = "Unsupported MIDI event type: " + type;
    return false;
}

bool parseSchedule(
        const var& command,
        OfflineGraphAudioRequest& request,
        String& error) {
    const var events = property(command, "events");
    if (const Array<var>* eventArray = events.getArray()) {
        for (const auto& event : *eventArray) {
            if (!parseEvent(event, request, error)) {
                return false;
            }
        }
    }

    const var noteValue = property(command, "note");
    if (noteValue.isVoid()) {
        return true;
    }

    const int channel = jlimit(1, 16, (int) doubleProperty(command, "channel", 1.0));
    const int note = jlimit(0, 127, (int) noteValue);
    const float velocity = jlimit(0.f, 1.f, (float) doubleProperty(command, "velocity", 0.8));
    const double durationMs = doubleProperty(command, "noteDurationMs", 800.0);
    const size_t noteOffSample = (size_t) jlimit<int64>(
            0,
            (int64) request.sampleCount,
            (int64) std::round(durationMs * request.sampleRate / 1000.0));
    addEvent(request, 0, MidiMessage::noteOn(channel, note, velocity));
    addEvent(request, noteOffSample, MidiMessage::noteOff(channel, note));
    return true;
}

bool parseRequest(
        const var& command,
        OfflineGraphAudioRequest& request,
        String& error) {
    request.sampleRate = doubleProperty(command, "sampleRate", 44100.0);
    request.blockSize = jlimit(16, 8192, (int) doubleProperty(command, "blockSize", 512.0));
    request.channelCount = jlimit(1, 2, (int) doubleProperty(command, "channels", 2.0));
    request.voiceDurationSeconds = (float) doubleProperty(command, "voiceDurationSeconds", 7.0);
    request.outputGain = jlimit(
            0.f,
            16.f,
            (float) doubleProperty(command, "outputGain", 0.125));
    request.controlNoteOffset = jlimit(
            -127,
            127,
            (int) doubleProperty(command, "controlNoteOffset", 0.0));
    const var randomSeed = property(command, "randomSeed");
    if (!randomSeed.isVoid()) {
        request.randomSeed = (int64_t) randomSeed;
        request.hasRandomSeed = true;
    }
    const String ratePolicy = stringProperty(command, "ratePolicy", "native");
    if (ratePolicy == "native") {
        request.ratePolicy = OfflineGraphAudioRatePolicy::Native;
    } else if (ratePolicy == "legacyInternal44100") {
        request.ratePolicy = OfflineGraphAudioRatePolicy::LegacyInternal44100;
    } else {
        error = "Unsupported offline audio rate policy: " + ratePolicy;
        return false;
    }
    const double durationMs = jlimit(1.0, 60000.0, doubleProperty(command, "durationMs", 1000.0));

    if (request.sampleRate <= 0.0) {
        error = "Sample rate must be positive";
        return false;
    }

    request.sampleCount = (size_t) jmax<int64>(
            1,
            (int64) std::round(durationMs * request.sampleRate / 1000.0));
    return parseSchedule(command, request, error);
}

var captureMetrics(
        const OfflineGraphAudioResult& capture,
        const OfflineGraphAudioRequest& request) {
    var metrics = new DynamicObject();
    auto* object = metrics.getDynamicObject();
    Array<var> channelMetrics;
    double sumSquares = 0.0;
    float peak = 0.f;

    for (int channel = 0; channel < request.channelCount; ++channel) {
        const auto& channelSamples = capture.channels[(size_t) channel];
        Buffer<float> samples(const_cast<float*>(channelSamples.data()), (int) channelSamples.size());
        ScopedAlloc<float> magnitudes((int) channelSamples.size());
        samples.copyTo(magnitudes);
        magnitudes.abs();

        const float channelPeak = magnitudes.max();
        const double channelNorm = (double) samples.normL2();
        const double channelRms = channelNorm / std::sqrt((double) request.sampleCount);
        peak = jmax(peak, channelPeak);
        sumSquares += channelNorm * channelNorm;

        var channelMetric = new DynamicObject();
        channelMetric.getDynamicObject()->setProperty("channel", channel);
        channelMetric.getDynamicObject()->setProperty("peak", channelPeak);
        channelMetric.getDynamicObject()->setProperty("rms", channelRms);
        channelMetrics.add(channelMetric);
    }

    object->setProperty("sampleRate", request.sampleRate);
    object->setProperty("channels", request.channelCount);
    object->setProperty("samples", (int64) request.sampleCount);
    object->setProperty("durationMs", 1000.0 * (double) request.sampleCount / request.sampleRate);
    object->setProperty("peak", peak);
    object->setProperty(
            "rms",
            std::sqrt(sumSquares / (double) (request.channelCount * request.sampleCount)));
    object->setProperty("channelMetrics", channelMetrics);
    return metrics;
}

bool writeCapture(
        const File& path,
        const OfflineGraphAudioResult& capture,
        const OfflineGraphAudioRequest& request,
        String& error) {
    if (path == File()) {
        return true;
    }

    AudioSampleBuffer buffer(request.channelCount, (int) request.sampleCount);
    for (int channel = 0; channel < request.channelCount; ++channel) {
        Buffer<float>(
                const_cast<float*>(capture.channels[(size_t) channel].data()),
                (int) request.sampleCount).copyTo(Buffer<float>(buffer, channel));
    }

    path.getParentDirectory().createDirectory();
    std::unique_ptr<FileOutputStream> stream(path.createOutputStream());
    if (stream == nullptr || !stream->openedOk()) {
        error = "Could not open audio capture path: " + path.getFullPathName();
        return false;
    }

    WavAudioFormat wavFormat;
    std::unique_ptr<AudioFormatWriter> writer(wavFormat.createWriterFor(
            stream.get(),
            request.sampleRate,
            (uint32) request.channelCount,
            24,
            {},
            0));
    if (writer == nullptr) {
        error = "Could not create WAV writer: " + path.getFullPathName();
        return false;
    }

    stream.release();
    if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples())) {
        error = "Could not write WAV capture: " + path.getFullPathName();
        return false;
    }
    return true;
}

bool writeRawCapture(
        const File& path,
        const OfflineGraphAudioResult& capture,
        const OfflineGraphAudioRequest& request,
        String& error) {
    if (path == File()) {
        return true;
    }

    path.getParentDirectory().createDirectory();
    std::unique_ptr<FileOutputStream> stream(path.createOutputStream());
    if (stream == nullptr || !stream->openedOk()
            || !stream->setPosition(0) || stream->truncate().failed()) {
        error = "Could not open raw audio capture path: " + path.getFullPathName();
        return false;
    }

    const size_t byteCount = request.sampleCount * sizeof(float);
    for (int channel = 0; channel < request.channelCount; ++channel) {
        if (!stream->write(capture.channels[(size_t) channel].data(), byteCount)) {
            error = "Could not write raw audio capture: " + path.getFullPathName();
            return false;
        }
    }
    return true;
}

}

bool OfflineAudioCaptureAutomation::isScheduledCapture(const var& command) {
    const DynamicObject* object = command.getDynamicObject();
    if (object == nullptr) {
        return false;
    }

    const Identifier properties[] {
            "sampleRate",
            "blockSize",
            "channels",
            "durationMs",
            "voiceDurationSeconds",
            "outputGain",
            "controlNoteOffset",
            "ratePolicy",
            "events",
            "note",
            "noteDurationMs"
    };
    for (const auto& name : properties) {
        if (object->hasProperty(name)) {
            return true;
        }
    }
    return false;
}

bool OfflineAudioCaptureAutomation::capture(
        const var& command,
        const File& path,
        GraphExecutionPlan plan,
        uint64_t revision,
        var& data,
        String& error) {
    OfflineGraphAudioRequest request;
    if (!parseRequest(command, request, error)) {
        return false;
    }

    constexpr int maximumSpectralStageValues = 131072;
    const File stageCapturePath(stringProperty(command, "stageCapturePath"));
    CycleDsp::SpectralStageCaptureRecorder stageCapture;
    if (stageCapturePath != File()) {
        const size_t targetFrame = (size_t) jmax(
                0,
                (int) doubleProperty(command, "stageCaptureFrameIndex", 0.0));
        const int targetOccurrence = jmax(
                0,
                (int) doubleProperty(command, "stageCaptureOccurrenceIndex", 0.0));
        if (!stageCapture.prepare(
                maximumSpectralStageValues,
                targetFrame,
                targetOccurrence)) {
            error = "Could not prepare spectral stage capture";
            return false;
        }
        request.spectralStageCapture = &stageCapture;
    }

    const OfflineGraphAudioResult capture = OfflineGraphAudioRenderer::render(
            std::move(plan),
            revision,
            request);
    if (!capture.succeeded) {
        error = capture.error;
        return false;
    }
    if (!writeCapture(path, capture, request, error)) {
        return false;
    }
    const File rawPath(stringProperty(command, "rawPath"));
    if (!writeRawCapture(rawPath, capture, request, error)) {
        return false;
    }
    if (stageCapturePath != File() && !stageCapture.write(stageCapturePath, error)) {
        return false;
    }

    data = captureMetrics(capture, request);
    DynamicObject* object = data.getDynamicObject();
    object->setProperty("path", path == File() ? String {} : path.getFullPathName());
    object->setProperty(
            "rawPath",
            rawPath == File() ? String {} : rawPath.getFullPathName());
    object->setProperty(
            "stageCapturePath",
            stageCapturePath == File() ? String {} : stageCapturePath.getFullPathName());
    object->setProperty("events", (int) request.events.size());
    object->setProperty("blockSize", request.blockSize);
    object->setProperty("voiceDurationSeconds", request.voiceDurationSeconds);
    object->setProperty("controlNoteOffset", request.controlNoteOffset);
    if (request.hasRandomSeed) {
        object->setProperty("randomSeed", request.randomSeed);
    }
    object->setProperty(
            "ratePolicy",
            request.ratePolicy == OfflineGraphAudioRatePolicy::LegacyInternal44100
                    ? "legacyInternal44100"
                    : "native");
    object->setProperty("renderer", "realtimeGraphRenderer");
    return true;
}

}
