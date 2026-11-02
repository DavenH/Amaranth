#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "Runtime/SignalProbeWaveformCapture.h"

namespace CycleV2 {

namespace {

constexpr double kSampleRate = 44100.;
constexpr size_t kBlockSize = 256;
constexpr size_t kRetainedCaptures = 16;

class ProbeCapture final : public GraphProcessObserver {
public:
    struct Output {
        size_t outputIndex {};
        std::shared_ptr<std::vector<float>> samples;
        bool captured {};
    };

    void addOutput(const String& nodeId, size_t outputIndex, size_t length) {
        outputsByNode[nodeId].push_back(outputs.size());
        outputs.push_back({ outputIndex, std::make_shared<std::vector<float>>(length, 0.f) });
    }

    void nodeProcessed(const String& id, const AudioProcessContext& context) override {
        const auto found = outputsByNode.find(id);
        if (found == outputsByNode.end()) {
            return;
        }
        for (const auto index : found->second) {
            auto& output = outputs[index];
            if (offset >= output.samples->size()) {
                continue;
            }
            // Boundary nodes expose the mixed signal in the prepared output view.
            const auto* payload = output.outputIndex < context.outputs.size()
                    ? &context.outputs[output.outputIndex]
                    : output.outputIndex < context.outputViews.size()
                            ? context.outputViews[output.outputIndex] : nullptr;
            if (payload == nullptr || payload->domain != PortDomain::TimeSignal
                    || payload->block.samples.size() < context.frameCount) {
                continue;
            }
            const size_t frames = jmin(context.frameCount, output.samples->size() - offset);
            Buffer<float> target(output.samples->data() + offset, (int) frames);
            Buffer<float>(const_cast<float*>(payload->block.samples.data()), target.size()).copyTo(target);
            if (payload->isStereo() && payload->secondaryBlock.samples.size() >= frames) {
                target.add(Buffer<float>(const_cast<float*>(payload->secondaryBlock.samples.data()), target.size()));
            }
            output.captured = true;
        }
    }

    std::vector<Output> outputs;
    size_t offset {};

private:
    std::unordered_map<String, std::vector<size_t>, GraphDependencyIndex::StringHash> outputsByNode;
};

void audition(const GraphExecutionPlan& plan, ProbeCapture& capture, size_t length,
        size_t noteOff, int midiNote, const std::array<int, 128>& controllers) {
    GraphAudioExecutor executor;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = kBlockSize;
    spec.prewarmTraversalGrid = false;
    executor.prepareExecution(plan, spec);
    AudioVoiceContext voice;
    voice.controls.noteNumber = midiNote;
    for (size_t index = 0; index < controllers.size(); ++index) {
        voice.controls.controllers[index] = (float) controllers[index] / 127.f;
    }
    voice.controls.normalizedVoiceTimeIncrement = 1. / kSampleRate;
    voice.controls.normalizedVolumeEnvelopeTimeIncrement = 1. / kSampleRate;
    for (size_t start = 0; start < length; start += kBlockSize) {
        const size_t frames = jmin(kBlockSize, length - start);
        voice.controls.normalizedVoiceTime = (float) jmin(1., (double) start / kSampleRate);
        voice.events.clear();
        if (start == 0) {
            voice.events.push_back({ NoteLifecycleType::NoteOn, 0, voice.voiceIndex });
        }
        if (noteOff >= start && noteOff < start + frames) {
            voice.events.push_back({ NoteLifecycleType::NoteOff, noteOff - start, voice.voiceIndex });
        }
        capture.offset = start;
        executor.processRealtime(plan, frames, {}, voice, &capture);
    }
}

}

std::vector<SignalProbeWaveformCapture::Samples> SignalProbeWaveformCapture::capture(
        const GraphExecutionPlan& plan, const std::vector<Request>& requests,
        int midiNote, const std::array<int, 128>& controllers) {
    const juce::ScopedLock guard(lock);
    std::vector<Samples> result(requests.size());
    std::vector<Key> keys;
    std::vector<size_t> missing;
    std::vector<std::pair<size_t, size_t>> aliases;
    keys.reserve(requests.size());
    for (size_t index = 0; index < requests.size(); ++index) {
        const auto& request = requests[index];
        keys.push_back({ request.address.sourceStepIndex, request.address.sourceOutputIndex,
                (size_t) std::ceil((1. + jmax(0., request.releaseDuration)) * kSampleRate),
                (size_t) std::llround(jlimit(0., 1., request.noteOffTime) * kSampleRate),
                jlimit(0, 127, midiNote), controllers });
        const auto& key = keys.back();
        if (key.sourceStep < 0 || (size_t) key.sourceStep >= plan.steps.size()
                || key.sourceOutput < 0) {
            continue;
        }
        const auto found = std::find_if(entries.begin(), entries.end(),
                [&](const Entry& entry) { return entry.key == key; });
        if (found != entries.end()) {
            result[index] = found->samples;
        } else {
            const auto duplicate = std::find(keys.begin(), keys.end() - 1, key);
            if (duplicate != keys.end() - 1) {
                aliases.emplace_back(index, (size_t) std::distance(keys.begin(), duplicate));
            } else {
                missing.push_back(index);
            }
        }
    }
    while (!missing.empty()) {
        const auto group = keys[missing.front()];
        ProbeCapture capture;
        std::vector<size_t> destinations;
        for (auto iterator = missing.begin(); iterator != missing.end();) {
            const auto index = *iterator;
            const auto& key = keys[index];
            if (key.noteOff != group.noteOff || key.length != group.length) {
                ++iterator;
                continue;
            }
            capture.addOutput(plan.steps[(size_t) key.sourceStep].nodeId,
                    (size_t) key.sourceOutput, key.length);
            destinations.push_back(index);
            iterator = missing.erase(iterator);
        }
        audition(plan, capture, group.length, group.noteOff, group.midiNote, group.controllers);
        ++executions;
        for (size_t output = 0; output < destinations.size(); ++output) {
            if (!capture.outputs[output].captured) {
                continue;
            }
            const auto index = destinations[output];
            result[index] = capture.outputs[output].samples;
            if (entries.size() == kRetainedCaptures) {
                entries.erase(entries.begin());
            }
            entries.push_back({ keys[index], result[index] });
        }
    }
    for (const auto& [destination, source] : aliases) {
        result[destination] = result[source];
    }
    return result;
}

uint64_t SignalProbeWaveformCapture::executionCount() const {
    const juce::ScopedLock guard(lock);
    return executions;
}

}
