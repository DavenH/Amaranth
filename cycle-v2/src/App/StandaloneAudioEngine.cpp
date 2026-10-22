#include "App/StandaloneAudioEngine.h"

#include <algorithm>

#include <Algo/Resampling.h>
#include <Array/Buffer.h>

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
    releaseMidiSource(MidiEventSource::PatternPlayback);
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
    activeReference = nullptr;
    pendingReference.store(nullptr, std::memory_order_release);
    referenceOwners.clear();
    referencePlaying.store(false, std::memory_order_release);
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

void StandaloneAudioEngine::setVoiceDurationSeconds(float durationSeconds) {
    renderer.setVoiceDurationSeconds(durationSeconds);
}

void StandaloneAudioEngine::setGraphOutputGain(float gain) {
    renderer.setGraphOutputGain(gain);
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

var StandaloneAudioEngine::audioPerformance() {
    audioPerformanceMetrics.serviceNonRealtime();
    return audioPerformanceMetrics.toVar();
}

void StandaloneAudioEngine::resetAudioPerformance() {
    audioPerformanceMetrics.resetAndEnable();
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

bool StandaloneAudioEngine::startReferenceAudition(const PitchedSample& sample) {
    if (sample.size() == 0 || sample.samplerate <= 0) {
        return false;
    }
    const double rate = currentSampleRate.load(std::memory_order_acquire);
    if (rate <= 0.0) {
        return false;
    }
    const auto found = std::find_if(referenceOwners.begin(), referenceOwners.end(),
            [&](const auto& owner) {
                return owner->sourcePath == sample.lastLoadedFilePath
                        && owner->outputSampleRate == rate;
            });
    if (found != referenceOwners.end()) {
        referenceStopRequested.store(false, std::memory_order_release);
        pendingReference.store(found->get(), std::memory_order_release);
        referencePlaying.store(true, std::memory_order_release);
        return true;
    }

    auto prepared = std::make_unique<PreparedReference>();
    prepared->sourcePath = sample.lastLoadedFilePath;
    prepared->outputSampleRate = rate;
    const int count = juce::jmax(2,
            juce::roundToInt((double) sample.size() * rate / sample.samplerate));
    prepared->left.resize((size_t) count);
    prepared->right.resize((size_t) count);
    Resampling::linResample(sample.audio.left,
            Buffer<float>(prepared->left.data(), count));
    const Buffer<float> right = sample.audio.numChannels > 1
            ? sample.audio.right : sample.audio.left;
    Resampling::linResample(right,
            Buffer<float>(prepared->right.data(), count));

    PreparedReference* pointer = prepared.get();
    referenceOwners.push_back(std::move(prepared));
    referenceStopRequested.store(false, std::memory_order_release);
    pendingReference.store(pointer, std::memory_order_release);
    referencePlaying.store(true, std::memory_order_release);
    return true;
}

void StandaloneAudioEngine::stopReferenceAudition() {
    referenceStopRequested.store(true, std::memory_order_release);
    referencePlaying.store(false, std::memory_order_release);
}

bool StandaloneAudioEngine::enqueueMidiMessage(
        const MidiMessage& message,
        MidiEventSource source) {
    return enqueueMidiMessageAt(message, source, currentTimeSeconds());
}

bool StandaloneAudioEngine::enqueueMidiMessageAt(
        const MidiMessage& message,
        MidiEventSource source,
        double timestampSeconds) {
    if (midiRecordingEnabled.load(std::memory_order_relaxed)) {
        recordedMidiEvents.enqueue(message, source, timestampSeconds);
    }
    return midiEvents.enqueue(message, source, timestampSeconds);
}

void StandaloneAudioEngine::releaseMidiSource(MidiEventSource source) {
    if (source == MidiEventSource::PatternPlayback) {
        midiEvents.cancelSource(source);
    }
    const double timestamp = currentTimeSeconds();
    for (int channel = 1; channel <= 16; ++channel) {
        midiEvents.enqueue(
                source == MidiEventSource::PatternPlayback
                        ? MidiMessage::allSoundOff(channel)
                        : MidiMessage::allNotesOff(channel),
                source,
                timestamp);
    }
}

void StandaloneAudioEngine::audioDeviceIOCallbackWithContext(
        const float* const*,
        int,
        float* const* outputChannelData,
        int outputChannelCount,
        int frameCount,
        const AudioIODeviceCallbackContext&) {
    AudioPerformanceMetrics::RealtimeSample* measuredSample
            = audioPerformanceMetrics.beginRealtimeSample(
                    realtimePerformanceSample,
                    frameCount,
                    currentSampleRate.load(std::memory_order_relaxed))
            ? &realtimePerformanceSample
            : nullptr;
    {
        AudioPerformanceMetrics::ScopedRealtimeStage stage(
                measuredSample,
                AudioPerformanceMetrics::Stage::GraphAdoption);
        adoptPendingGraph();
    }

    const uint64_t callback = renderer.process(
            midiEvents,
            outputChannelData,
            outputChannelCount,
            frameCount,
            currentSampleRate.load(std::memory_order_relaxed),
            currentTimeSeconds(),
            measuredSample);
    renderReferenceAudition(outputChannelData, outputChannelCount, frameCount);

    {
        AudioPerformanceMetrics::ScopedRealtimeStage stage(
                measuredSample,
                AudioPerformanceMetrics::Stage::LiveCapture);
        liveCapture.append(
                outputChannelData,
                outputChannelCount,
                frameCount,
                callback);
    }
    if (measuredSample != nullptr) {
        audioPerformanceMetrics.publishRealtimeSample(*measuredSample);
    }
}

void StandaloneAudioEngine::renderReferenceAudition(
        float* const* outputChannelData,
        int outputChannelCount,
        int frameCount) {
    if (referenceStopRequested.exchange(false, std::memory_order_acq_rel)) {
        activeReference = nullptr;
        pendingReference.store(nullptr, std::memory_order_release);
        referencePlaying.store(false, std::memory_order_release);
        return;
    }
    if (PreparedReference* pending = pendingReference.exchange(
                nullptr, std::memory_order_acq_rel)) {
        activeReference = pending;
        referencePosition = 0;
    }
    if (activeReference == nullptr || outputChannelCount <= 0) {
        return;
    }

    const size_t remaining = activeReference->left.size() - referencePosition;
    const int count = (int) juce::jmin((size_t) frameCount, remaining);
    if (count > 0) {
        if (outputChannelData[0] != nullptr) {
            Buffer<float>(activeReference->left.data() + referencePosition, count)
                    .copyTo(Buffer<float>(outputChannelData[0], count));
        }
        if (outputChannelCount > 1 && outputChannelData[1] != nullptr) {
            Buffer<float>(activeReference->right.data() + referencePosition, count)
                    .copyTo(Buffer<float>(outputChannelData[1], count));
        }
        referencePosition += (size_t) count;
    }
    if (referencePosition >= activeReference->left.size()) {
        activeReference = nullptr;
        referencePlaying.store(false, std::memory_order_release);
    }
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
    renderer.resetVoices(true);
}

void StandaloneAudioEngine::handleIncomingMidiMessage(
        MidiInput*,
        const MidiMessage& message) {
    const double now = currentTimeSeconds();
    if (midiRecordingEnabled.load(std::memory_order_relaxed)) {
        recordedMidiEvents.enqueue(message, MidiEventSource::Hardware, now);
    }
    midiEvents.enqueue(
            message,
            MidiEventSource::Hardware,
            now);
}

void StandaloneAudioEngine::timerCallback() {
    audioPerformanceMetrics.serviceNonRealtime();
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
