#include "App/StandaloneAudioEngine.h"

#include <Array/Buffer.h>

#include <algorithm>
#include <cmath>

namespace CycleV2 {

using namespace juce;

namespace {

std::unique_ptr<PropertiesFile> createDeviceProperties() {
    PropertiesFile::Options options;
    options.applicationName = "CycleV2Audio";
    options.folderName = "Amaranth Audio/Cycle V2";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = PropertiesFile::storeAsXML;
    return std::make_unique<PropertiesFile>(options);
}

}

StandaloneAudioEngine::StandaloneAudioEngine() :
        deviceProperties(createDeviceProperties()) {
    startTimerHz(30);
}

StandaloneAudioEngine::~StandaloneAudioEngine() {
    stopTimer();
    stop();
}

bool StandaloneAudioEngine::start() {
    if (ready.load(std::memory_order_acquire)) {
        return true;
    }

    std::unique_ptr<XmlElement> savedState;
    if (deviceProperties != nullptr) {
        savedState = parseXML(deviceProperties->getValue("audioDeviceState"));
    }
    deviceError = deviceManager.initialise(0, 2, savedState.get(), true);
    AudioIODevice* device = deviceManager.getCurrentAudioDevice();
    if (deviceError.isNotEmpty() || device == nullptr) {
        if (deviceError.isEmpty()) {
            deviceError = "No audio output device is available";
        }
        ready.store(false, std::memory_order_release);
        return false;
    }

    for (const auto& input : MidiInput::getAvailableDevices()) {
        deviceManager.setMidiInputDeviceEnabled(input.identifier, true);
    }
    currentDeviceName = device->getName();
    deviceError = {};
    deviceManager.addMidiInputDeviceCallback({}, this);
    deviceManager.addAudioCallback(this);
    return true;
}

void StandaloneAudioEngine::stop() {
    releaseMidiSource(MidiEventSource::PerformanceKeyboard);
    releaseMidiSource(MidiEventSource::Hardware);
    deviceManager.removeMidiInputDeviceCallback({}, this);
    deviceManager.removeAudioCallback(this);
    if (deviceProperties != nullptr) {
        if (auto state = deviceManager.createStateXml()) {
            deviceProperties->setValue("audioDeviceState", state->toString());
            deviceProperties->saveIfNeeded();
        }
    }
    deviceManager.closeAudioDevice();
    ready.store(false, std::memory_order_release);
    renderer.setPreparedGraph(nullptr);
    activeGraph = nullptr;
    pendingGraph.store(nullptr, std::memory_order_release);
    retiredGraph.store(nullptr, std::memory_order_release);
    graphOwners.clear();
}

bool StandaloneAudioEngine::publishGraph(
        GraphExecutionPlan plan,
        uint64_t revision) {
    AudioExecutionSpec spec;
    spec.maximumFrameCount = (size_t) jmax(1, currentBlockSize.load(std::memory_order_acquire));
    spec.sampleRate = currentSampleRate.load(std::memory_order_acquire);
    spec.channelLayout = ChannelLayout::LinkedStereo;
    auto graph = RealtimeGraphRenderer::prepareGraph(
            std::move(plan),
            revision,
            spec);
    PreparedGraph* graphPointer = graph.get();
    graphOwners.push_back(std::move(graph));

    PreparedGraph* superseded = pendingGraph.exchange(
            graphPointer,
            std::memory_order_acq_rel);
    reclaimGraph(superseded);
    return true;
}

StandaloneAudioEngine::Status StandaloneAudioEngine::status() const {
    return {
            ready.load(std::memory_order_acquire),
            currentDeviceName,
            deviceError,
            currentSampleRate.load(std::memory_order_acquire),
            currentBlockSize.load(std::memory_order_acquire),
            devicePreparationRevision.load(std::memory_order_acquire),
            renderer.diagnostics(midiEvents)
    };
}

StandaloneAudioEngine::LiveCapture StandaloneAudioEngine::captureLiveAudio(
        int durationMs) {
    LiveCapture capture;
    capture.sampleRate = currentSampleRate.load(std::memory_order_acquire);
    if (!ready.load(std::memory_order_acquire) || capture.sampleRate <= 0.) {
        return capture;
    }

    const size_t requestedFrames = jlimit(
            (size_t) 1,
            liveCaptureCapacity,
            (size_t) roundToInt(capture.sampleRate * (double) jmax(1, durationMs) / 1000.0));
    liveCapturePosition.store(0, std::memory_order_relaxed);
    liveCaptureFirstCallback.store(
            renderer.diagnostics(midiEvents).callbackCount + 1,
            std::memory_order_relaxed);
    liveCaptureLastCallback.store(0, std::memory_order_relaxed);
    liveCaptureTarget.store(requestedFrames, std::memory_order_release);

    const uint32 timeoutMs = (uint32) jlimit(100, 5000, durationMs * 2 + 500);
    const uint32 started = Time::getMillisecondCounter();
    while (liveCaptureTarget.load(std::memory_order_acquire) != 0
            && Time::getMillisecondCounter() - started < timeoutMs) {
        Thread::sleep(5);
    }

    const size_t capturedFrames = liveCapturePosition.load(std::memory_order_acquire);
    capture.completed = liveCaptureTarget.load(std::memory_order_acquire) == 0
            && capturedFrames >= requestedFrames;
    if (!capture.completed) {
        liveCaptureTarget.store(0, std::memory_order_release);
        return capture;
    }

    capture.firstCallback = liveCaptureFirstCallback.load(std::memory_order_acquire);
    capture.lastCallback = liveCaptureLastCallback.load(std::memory_order_acquire);
    capture.left.assign(liveCaptureLeft.begin(), liveCaptureLeft.begin() + (int) requestedFrames);
    capture.right.assign(liveCaptureRight.begin(), liveCaptureRight.begin() + (int) requestedFrames);

    Buffer<float> left(capture.left.data(), (int) capture.left.size());
    Buffer<float> right(capture.right.data(), (int) capture.right.size());
    const double leftNorm = left.normL2();
    const double rightNorm = right.normL2();
    const double squaredNorm = leftNorm * leftNorm + rightNorm * rightNorm;
    std::vector<float> absolute = capture.left;
    Buffer<float> absoluteBuffer(absolute.data(), (int) absolute.size());
    absoluteBuffer.abs();
    capture.peak = absoluteBuffer.max();
    absolute = capture.right;
    absoluteBuffer = Buffer<float>(absolute.data(), (int) absolute.size());
    absoluteBuffer.abs();
    capture.peak = jmax(capture.peak, absoluteBuffer.max());
    capture.rms = (float) std::sqrt(
            squaredNorm / (double) (requestedFrames * 2));
    return capture;
}

bool StandaloneAudioEngine::enqueueMidiMessage(
        const MidiMessage& message,
        MidiEventSource source) {
    return midiEvents.enqueue(message, source, currentTimeSeconds());
}

void StandaloneAudioEngine::releaseMidiSource(MidiEventSource source) {
    midiEvents.enqueue(
            MidiMessage::allNotesOff(1),
            source,
            currentTimeSeconds());
}

void StandaloneAudioEngine::audioDeviceIOCallbackWithContext(
        const float* const*,
        int,
        float* const* outputChannelData,
        int outputChannelCount,
        int frameCount,
        const AudioIODeviceCallbackContext&) {
    adoptPendingGraph();
    renderer.process(
            midiEvents,
            outputChannelData,
            outputChannelCount,
            frameCount,
            currentSampleRate.load(std::memory_order_relaxed),
            currentTimeSeconds());
    captureOutput(outputChannelData, outputChannelCount, frameCount);
}

void StandaloneAudioEngine::audioDeviceAboutToStart(AudioIODevice* device) {
    if (device == nullptr) {
        return;
    }
    currentSampleRate.store(device->getCurrentSampleRate(), std::memory_order_release);
    currentBlockSize.store(device->getCurrentBufferSizeSamples(), std::memory_order_release);
    devicePreparationRevision.fetch_add(1, std::memory_order_acq_rel);
    ready.store(true, std::memory_order_release);
}

void StandaloneAudioEngine::audioDeviceStopped() {
    ready.store(false, std::memory_order_release);
    renderer.resetVoices();
}

void StandaloneAudioEngine::handleIncomingMidiMessage(
        MidiInput*,
        const MidiMessage& message) {
    midiEvents.enqueue(
            message,
            MidiEventSource::Hardware,
            currentTimeSeconds());
}

void StandaloneAudioEngine::timerCallback() {
    PreparedGraph* retired = retiredGraph.exchange(nullptr, std::memory_order_acq_rel);
    reclaimGraph(retired);
    for (const auto& graph : graphOwners) {
        graph->executor.serviceNonRealtimePreparation();
    }
}

void StandaloneAudioEngine::adoptPendingGraph() {
    if (retiredGraph.load(std::memory_order_acquire) != nullptr) {
        return;
    }

    PreparedGraph* pending = pendingGraph.exchange(nullptr, std::memory_order_acq_rel);
    if (pending == nullptr) {
        return;
    }

    PreparedGraph* previous = activeGraph;
    activeGraph = pending;
    renderer.setPreparedGraph(activeGraph);
    if (previous != nullptr) {
        retiredGraph.store(previous, std::memory_order_release);
    }
}

void StandaloneAudioEngine::reclaimGraph(PreparedGraph* graph) {
    if (graph == nullptr) {
        return;
    }
    const auto found = std::find_if(
            graphOwners.begin(),
            graphOwners.end(),
            [&](const auto& owner) { return owner.get() == graph; });
    if (found != graphOwners.end()) {
        graphOwners.erase(found);
    }
}

void StandaloneAudioEngine::captureOutput(
        float* const* outputChannelData,
        int outputChannelCount,
        int frameCount) {
    const size_t target = liveCaptureTarget.load(std::memory_order_acquire);
    if (target == 0 || frameCount <= 0) {
        return;
    }
    const size_t position = liveCapturePosition.load(std::memory_order_relaxed);
    if (position >= target) {
        return;
    }

    const int copyCount = (int) jmin((size_t) frameCount, target - position);
    Buffer<float> left(liveCaptureLeft.data() + position, copyCount);
    Buffer<float> right(liveCaptureRight.data() + position, copyCount);
    if (outputChannelCount > 0 && outputChannelData[0] != nullptr) {
        Buffer<float>(outputChannelData[0], copyCount).copyTo(left);
    } else {
        left.zero();
    }
    if (outputChannelCount > 1 && outputChannelData[1] != nullptr) {
        Buffer<float>(outputChannelData[1], copyCount).copyTo(right);
    } else {
        left.copyTo(right);
    }

    const size_t nextPosition = position + (size_t) copyCount;
    liveCapturePosition.store(nextPosition, std::memory_order_release);
    if (nextPosition >= target) {
        liveCaptureLastCallback.store(
                renderer.diagnostics(midiEvents).callbackCount,
                std::memory_order_relaxed);
        liveCaptureTarget.store(0, std::memory_order_release);
    }
}

double StandaloneAudioEngine::currentTimeSeconds() const {
    return Time::getMillisecondCounterHiRes() / 1000.0;
}

}
