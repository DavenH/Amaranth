#include "App/StandaloneAudioEngine.h"

#include <algorithm>

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
    const double sampleRate = currentSampleRate.load(std::memory_order_acquire);
    if (!ready.load(std::memory_order_acquire)
            || !liveCapture.begin(sampleRate, durationMs)) {
        return {};
    }

    const uint32 timeoutMs = (uint32) jlimit(100, 5000, durationMs * 2 + 500);
    return liveCapture.waitForCompletion((int) timeoutMs);
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
    liveCapture.append(outputChannelData, outputChannelCount, frameCount);
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

double StandaloneAudioEngine::currentTimeSeconds() const {
    return Time::getMillisecondCounterHiRes() / 1000.0;
}

}
