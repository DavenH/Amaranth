#include <cmath>
#include <filesystem>

#include <UI/Panels/TimeSurfaceStyles.h>

#include "App/SpyMovieExport.h"
#include "Graph/GraphSerializer.h"
#include "Runtime/OfflineGraphAudioRenderer.h"
#include "UI/PatternPlaybackSchedule.h"
#include "UI/SpyFrameRenderer.h"

namespace CycleV2 {
using namespace juce;

Result SpyMovieExportSettings::validate(const PresetMidiSequence& sequence) const {
    if (outputDirectory == File() || outputDirectory.exists()) {
        return Result::fail("Choose a new output folder; existing folders are never replaced.");
    }
    if (!ffmpeg.existsAsFile()) {
        return Result::fail("Choose the ffmpeg executable.");
    }
    if (width < 64 || height < 64 || width > 7680 || height > 4320
            || width % 2 != 0 || height % 2 != 0 || fps < 1 || fps > 120
            || !std::isfinite(bitrateMbps) || bitrateMbps <= 0.0 || bitrateMbps > 200.0
            || !std::isfinite(tailSeconds) || tailSeconds < 0.0 || tailSeconds > 30.0) {
        return Result::fail("Invalid dimensions, frame rate, bitrate or release tail.");
    }
    if (sequence.notes.empty()) {
        return Result::fail("The linked phrase needs at least one note to export a movie with audio.");
    }
    if (!PresetPresentationCodec::readSequenceJSON(PresetPresentationCodec::writeSequenceJSON(sequence))) {
        return Result::fail("The linked phrase is invalid.");
    }
    if (sequence.durationSeconds + tailSeconds > 120.0) {
        return Result::fail("The phrase and release tail must fit within 120 seconds.");
    }
    if (gradient.isNotEmpty()) {
        bool found = false;
        for (const auto& entry : TimeSurfaceStyles::entries) {
            found = found || gradient == entry.id;
        }
        if (!found) {
            return Result::fail("Unknown gradient.");
        }
    }
    return Result::ok();
}

var SpyMovieExportSettings::toJSON(const String& preset, const PresetMidiSequence& sequence) const {
    auto object = std::make_unique<DynamicObject>();
    object->setProperty("preset", preset);
    object->setProperty("sequence", PresetPresentationCodec::writeSequenceJSON(sequence));
    object->setProperty("outputDirectory", outputDirectory.getFullPathName());
    object->setProperty("ffmpeg", ffmpeg.getFullPathName());
    object->setProperty("gradient", gradient);
    object->setProperty("width", width);
    object->setProperty("height", height);
    object->setProperty("fps", fps);
    object->setProperty("bitrateMbps", bitrateMbps);
    object->setProperty("tailSeconds", tailSeconds);
    return var(object.release());
}

File SpyMovieExportSettings::findFFmpeg() {
    StringArray locations;
    const auto path = SystemStats::getEnvironmentVariable("PATH", "");
    locations.addTokens(path, File::getSeparatorChar() == '\\' ? ";" : ":", "\"");
    locations.addArray({"/opt/homebrew/bin", "/usr/local/bin", "/usr/bin"});
    for (const auto& directory : locations) {
      #if JUCE_WINDOWS
        const auto executable = File::getCurrentWorkingDirectory().getChildFile(directory).getChildFile("ffmpeg.exe");
      #else
        const auto executable = File::getCurrentWorkingDirectory().getChildFile(directory).getChildFile("ffmpeg");
      #endif
        if (executable.existsAsFile()) {
            return executable;
        }
    }
    return {};
}

struct SpyMovieExportWorker::State {
    SpyMovieExportSettings settings;
    GraphLoadResult loaded;
    GraphPresentationModel presentation;
    std::vector<PatternPlaybackEvent> events;
    OfflineGraphAudioResult audio;
    SpyFrameRequest frame;
    ChildProcess encoder;
    size_t eventIndex {};
    int completed {};
    int count {};
    bool prepared {};
    bool ownsOutput {};
    bool encoding {};
};

SpyMovieExportWorker::SpyMovieExportWorker(File job) :
        jobFile(std::move(job))
    ,   state(std::make_unique<State>()) {}
SpyMovieExportWorker::~SpyMovieExportWorker() {
    if (state->encoder.isRunning()) {
        state->encoder.kill();
    }
}

void SpyMovieExportWorker::start() { startTimer(1); }
bool SpyMovieExportWorker::cancelled() const { return jobFile.getSiblingFile("cancel").existsAsFile(); }

void SpyMovieExportWorker::publish(const String& status, const String& message) {
    auto object = std::make_unique<DynamicObject>();
    object->setProperty("status", status);
    object->setProperty("message", message);
    object->setProperty("completedFrames", state->completed);
    object->setProperty("totalFrames", state->count);
    object->setProperty("width", state->settings.width);
    object->setProperty("height", state->settings.height);
    object->setProperty("fps", state->settings.fps);
    object->setProperty("gradient", state->settings.gradient);
    const auto json = JSON::toString(var(object.release()));
    const auto destination = jobFile.getSiblingFile("status.json");
    TemporaryFile temporary(destination);
    if (temporary.getFile().replaceWithText(json)) {
        temporary.overwriteTargetFileWithTemporary();
    }
    if (state->ownsOutput) {
        state->settings.outputDirectory.getChildFile("manifest.json").replaceWithText(json);
    }
}

void SpyMovieExportWorker::finish(const String& status, const String& message) {
    stopTimer();
    if (state->encoder.isRunning()) {
        state->encoder.kill();
    }
    publish(status, message);
    if (auto* application = JUCEApplication::getInstance()) {
        application->setApplicationReturnValue(status == "complete" ? 0 : 1);
        application->quit();
    }
}

bool SpyMovieExportWorker::prepare(String& error) {
    const auto job = JSON::parse(jobFile);
    const auto sequence = PresetPresentationCodec::readSequenceJSON(job["sequence"]);
    if (!sequence || !job["preset"].isString()) {
        error = "Invalid movie job or phrase.";
        return false;
    }
    auto& settings = state->settings;
    settings.outputDirectory = jobFile.getParentDirectory().getChildFile(job["outputDirectory"].toString());
    settings.ffmpeg = jobFile.getParentDirectory().getChildFile(job["ffmpeg"].toString());
    settings.gradient = job["gradient"].toString();
    settings.width = job["width"];
    settings.height = job["height"];
    settings.fps = job["fps"];
    settings.bitrateMbps = job["bitrateMbps"];
    settings.tailSeconds = job["tailSeconds"];
    if (const auto valid = settings.validate(*sequence); valid.failed()) {
        error = valid.getErrorMessage();
        return false;
    }
    state->loaded = GraphSerializer().loadJsonString(job["preset"].toString());
    if (!state->loaded.succeeded() || !state->presentation.refresh(state->loaded.graph, 1)) {
        error = "Could not prepare the preset snapshot.";
        return false;
    }
    state->presentation.setProbeNoteOffTimes(state->loaded.presentation.spyNoteOffTimes);
    auto expanded = PatternPlaybackSchedule::expand(*sequence, error);
    if (!expanded) {
        return false;
    }
    state->events = std::move(*expanded);
    // Decimal durations must not add a roundoff-only endpoint frame.
    state->count = (int) std::ceil((sequence->durationSeconds + settings.tailSeconds) * settings.fps - 1.e-9);
    state->frame.width = settings.width;
    state->frame.height = settings.height;
    for (const auto& entry : TimeSurfaceStyles::entries) {
        if (settings.gradient == entry.id) {
            state->frame.gradient = entry.style;
        }
    }
    for (const auto& event : state->events) {
        if (event.message.isNoteOn()) {
            state->frame.midiNote = event.message.getNoteNumber();
            break;
        }
    }
    std::error_code directoryError;
    if (!settings.outputDirectory.getParentDirectory().createDirectory()
            || !std::filesystem::create_directory(std::filesystem::u8path(
                    settings.outputDirectory.getFullPathName().toStdString()), directoryError)) {
        error = "Could not create a new output folder.";
        return false;
    }
    state->ownsOutput = true;
    const bool savedPreset = settings.outputDirectory.getChildFile("source-preset.cyclegraph").replaceWithText(job["preset"].toString());
    auto pattern = std::make_unique<DynamicObject>();
    pattern->setProperty("version", 1);
    pattern->setProperty("id", "export-snapshot");
    pattern->setProperty("name", "Export snapshot");
    pattern->setProperty("sequence", job["sequence"]);
    if (!settings.outputDirectory.getChildFile("source-pattern.cyclepattern").replaceWithText(
                JSON::toString(var(pattern.release()))) || !savedPreset) {
        error = "Could not write the input snapshots.";
        return false;
    }
    OfflineGraphAudioRequest request;
    request.sampleRate = 48000;
    request.voiceDurationSeconds = 0;
    request.keyScaleNoteOverride = *state->frame.midiNote;
    request.hasRandomSeed = true;
    request.randomSeed = 1729;
    request.sampleCount = (size_t) std::llround(state->count * request.sampleRate / settings.fps);
    for (int index = 0; index < state->count; ++index) {
        request.playbackSamples.push_back((size_t) std::llround(index * request.sampleRate / settings.fps));
    }
    for (const auto& event : state->events) {
        request.events.push_back({(size_t) std::llround(event.timeSeconds * request.sampleRate),
                event.message, MidiEventSource::PatternPlayback});
    }
    publish("audio", "Rendering synth audio");
    state->audio = OfflineGraphAudioRenderer::render(state->presentation.compileResult().plan, 1, request);
    if (!state->audio.succeeded) {
        error = state->audio.error;
        return false;
    }
    std::unique_ptr<FileOutputStream> stream(settings.outputDirectory.getChildFile("audio.wav").createOutputStream());
    if (stream == nullptr || !stream->openedOk()) {
        error = "Could not write synth audio.";
        return false;
    }
    std::unique_ptr<AudioFormatWriter> writer(WavAudioFormat().createWriterFor(stream.get(), 48000, 2, 24, {}, 0));
    if (writer == nullptr) {
        error = "Could not create WAV encoder.";
        return false;
    }
    stream.release();
    const float* channels[] {state->audio.channels[0].data(), state->audio.channels[1].data()};
    if (!writer->writeFromFloatArrays(channels, 2, (int) request.sampleCount)) {
        error = "Could not finish synth audio.";
        return false;
    }
    state->audio.channels = {};
    return true;
}

bool SpyMovieExportWorker::renderFrame(String& error) {
    const double time = (double) state->completed / state->settings.fps;
    while (state->eventIndex < state->events.size() && state->events[state->eventIndex].timeSeconds <= time) {
        const auto& message = state->events[state->eventIndex++].message;
        if (message.isController()) {
            state->frame.controllers[(size_t) message.getControllerNumber()] = message.getControllerValue();
        }
    }
    state->frame.playback = state->audio.playbackFrames[(size_t) state->completed];
    const auto rendered = SpyFrameRenderer::render(state->loaded.graph, state->presentation,
            state->loaded.presentation, state->frame, error);
    if (!rendered.image.isValid()) {
        return false;
    }
    state->settings.gradient = TimeSurfaceStyles::id(rendered.gradient);
    const auto destination = state->settings.outputDirectory.getChildFile(
            "frame-" + String(state->completed).paddedLeft('0', 6) + ".png");
    TemporaryFile temporary(destination);
    {
        FileOutputStream stream(temporary.getFile());
        if (!stream.openedOk() || !PNGImageFormat().writeImageToStream(rendered.image, stream)) {
            error = "Could not write PNG frame " + String(state->completed);
            return false;
        }
        stream.flush();
        if (stream.getStatus().failed()) {
            error = "Could not finish PNG frame " + String(state->completed);
            return false;
        }
    }
    if (!temporary.overwriteTargetFileWithTemporary()) {
        error = "Could not finish PNG frame " + String(state->completed);
        return false;
    }
    ++state->completed;
    publish("frames", "Rendering frame " + String(state->completed) + " / " + String(state->count));
    return true;
}

void SpyMovieExportWorker::timerCallback() {
    if (cancelled()) {
        finish("cancelled", "Export cancelled; completed files retained.");
        return;
    }
    String error;
    if (!state->prepared) {
        if (!prepare(error)) {
            finish("failed", error);
            return;
        }
        state->prepared = true;
    } else if (state->completed < state->count) {
        if (!renderFrame(error)) {
            finish("failed", error);
        }
    } else if (!state->encoding) {
        const auto& settings = state->settings;
        const auto output = settings.outputDirectory;
        const StringArray arguments {settings.ffmpeg.getFullPathName(), "-nostdin", "-hide_banner", "-loglevel", "error", "-n",
                "-framerate", String(settings.fps), "-start_number", "0", "-i", output.getChildFile("frame-%06d.png").getFullPathName(),
                "-i", output.getChildFile("audio.wav").getFullPathName(), "-map", "0:v:0", "-map", "1:a:0",
                "-t", String((double) state->count / settings.fps, 9), "-c:v", "libx264", "-b:v", String(std::llround(settings.bitrateMbps * 1000000)),
                "-preset", "slow", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "320k", "-movflags", "+faststart",
                output.getChildFile("sequence.partial.mp4").getFullPathName()};
        publish("encoding", "Encoding MP4 with ffmpeg");
        if (!state->encoder.start(arguments)) {
            finish("failed", "Could not launch ffmpeg.");
            return;
        }
        state->encoding = true;
        startTimer(100);
    } else if (!state->encoder.isRunning()) {
        const auto log = state->encoder.readAllProcessOutput();
        if (state->encoder.getExitCode() != 0) {
            finish("failed", "ffmpeg failed: " + log.substring(0, 1500));
        } else if (!state->settings.outputDirectory.getChildFile("sequence.partial.mp4").moveFileTo(
                    state->settings.outputDirectory.getChildFile("sequence.mp4"))) {
            finish("failed", "Could not publish the MP4.");
        } else {
            finish("complete", "Movie export complete.");
        }
    }
}

}
