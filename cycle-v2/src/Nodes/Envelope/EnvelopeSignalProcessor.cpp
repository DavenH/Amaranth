#include "EnvelopeSignalProcessor.h"

#include "../Effect2D/CurveNodeModels.h"

#include <Curve/Curve.h>

namespace CycleV2 {

EnvelopeSignalProcessor::EnvelopeSignalProcessor() :
        mesh      ("CycleV2EnvelopeDsp")
    ,   rasterizer(nullptr, "CycleV2EnvelopeDspRasterizer") {
    Curve::calcTable();
    props.active = true;
}

EnvelopeSignalProcessor::~EnvelopeSignalProcessor() {
    mesh.destroy();
}

std::shared_ptr<const EnvelopeConfiguration> EnvelopeSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters) {
    if (Curve::table == nullptr) {
        Curve::calcTable();
    }

    auto result = std::make_shared<EnvelopeConfiguration>();
    result->mesh = std::shared_ptr<EnvelopeMesh>(
            new EnvelopeMesh("CycleV2EnvelopeConfiguration"),
            [](EnvelopeMesh* meshToDelete) {
                meshToDelete->destroy();
                delete meshToDelete;
            });

    const String snapshot = CurveNodeModelCodec::envelopePayloadFromParameters(parameters);

    if (!EnvelopeMeshState::apply(snapshot, *result->mesh)) {
        return {};
    }

    result->rasterizer = std::make_shared<EnvRasterizer>(nullptr, "CycleV2EnvelopeConfigurationRasterizer");
    result->rasterizer->setMesh(result->mesh.get());
    result->redMorph = parameterFloat(parameters, "red", 0.5f);
    result->blueMorph = parameterFloat(parameters, "blue", 0.5f);
    result->rasterizer->setMorphPosition({
            0.f,
            result->redMorph,
            result->blueMorph
    });
    result->rasterizer->renderWaveformOnly(result->mesh.get(), 0.f);
    result->rasterizer->validateState();
    if (!result->rasterizer->canRasterizeWaveform()) {
        return {};
    }

    result->level = parameterFloat(parameters, "level", 1.f);
    result->logarithmic = parameterFloat(parameters, "logarithmic", 0.f) != 0.f;
    result->dynamicWhileLive = typedParameterBool(parameters, "dynamic", false);
    result->meshSnapshot = snapshot;
    return result;
}

void EnvelopeSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    const size_t maximumColumns = std::max(defaultTraversalColumns, spec.maximumFrameCount);
    traversalMemory.ensureSize((int) (2 * maximumColumns));

    if (configuration == nullptr || pendingRevision == adoptedRevision) {
        return;
    }

    const EnvelopeConfiguration* previous = preparedConfiguration();
    const bool freezeCurrentMorph = active
            && previous != nullptr
            && previous->dynamicWhileLive
            && !configuration->dynamicWhileLive;
    if (freezeCurrentMorph) {
        const int previousSlot = activeSlot.load(std::memory_order_acquire);
        activeConfiguration = previousSlot >= 0
                ? preparedSlots[(size_t) previousSlot].configuration
                : activeConfiguration;
    } else {
        activeConfiguration = configuration;
    }
    activeSlot.store(-1, std::memory_order_release);
    publishedSlot.store(-1, std::memory_order_release);
    requestedGeneration.store(0, std::memory_order_release);
    preparedGeneration.store(0, std::memory_order_release);
    adoptedDynamicGeneration.store(0, std::memory_order_release);
    hasRequestedMorph = false;
    playback.validate(activeConfiguration->rasterizer->preparedPlaybackView());
    props.logarithmic = configuration->logarithmic;
    level = configuration->level;
    modelReady = true;
    adoptedRevision = pendingRevision;
}

void EnvelopeSignalProcessor::adoptConfiguration(const PublishedNodeConfiguration& published) {
    if (published.revision == adoptedRevision || published.value == nullptr
            || published.value->role() != AudioModuleRole::Envelope) {
        return;
    }

    configuration = std::static_pointer_cast<const EnvelopeConfiguration>(published.value);
    pendingRevision = published.revision;
}

std::shared_ptr<const EnvelopeConfiguration> EnvelopeSignalProcessor::prepareDynamicConfiguration(
        const EnvelopeConfiguration& base,
        float red,
        float blue) {
    auto result = std::make_shared<EnvelopeConfiguration>();
    result->mesh = std::shared_ptr<EnvelopeMesh>(
            new EnvelopeMesh("CycleV2DynamicEnvelope"),
            [](EnvelopeMesh* meshToDelete) {
                meshToDelete->destroy();
                delete meshToDelete;
            });
    if (!EnvelopeMeshState::apply(base.meshSnapshot, *result->mesh)) {
        return {};
    }

    result->rasterizer = std::make_shared<EnvRasterizer>(
            nullptr, "CycleV2DynamicEnvelopeRasterizer");
    result->rasterizer->setMesh(result->mesh.get());
    result->rasterizer->setMorphPosition({ 0.f, red, blue });
    result->rasterizer->renderWaveformOnly(result->mesh.get(), 0.f);
    result->rasterizer->validateState();
    if (!result->rasterizer->canRasterizeWaveform()) {
        return {};
    }

    result->level = base.level;
    result->redMorph = red;
    result->blueMorph = blue;
    result->logarithmic = base.logarithmic;
    result->dynamicWhileLive = base.dynamicWhileLive;
    result->meshSnapshot = base.meshSnapshot;
    return result;
}

bool EnvelopeSignalProcessor::serviceNonRealtimePreparation() {
    const uint64_t firstSequence = requestedGeneration.load(std::memory_order_acquire);
    if ((firstSequence & 1u) != 0 || firstSequence == 0
            || firstSequence <= preparedGeneration.load(std::memory_order_acquire)
            || configuration == nullptr) {
        return false;
    }

    DynamicRequest request;
    request.generation = firstSequence;
    request.noteSerial = requestNoteSerial.load(std::memory_order_relaxed);
    request.red = requestedRed.load(std::memory_order_relaxed);
    request.blue = requestedBlue.load(std::memory_order_relaxed);
    if (requestedGeneration.load(std::memory_order_acquire) != firstSequence) {
        return false;
    }

    auto prepared = prepareDynamicConfiguration(
            *configuration, request.red, request.blue);
    if (prepared == nullptr) {
        return false;
    }
    if (requestedGeneration.load(std::memory_order_acquire) != firstSequence) {
        ++staleResultCount;
        return false;
    }

    const int published = publishedSlot.load(std::memory_order_acquire);
    const int activePrepared = activeSlot.load(std::memory_order_acquire);
    int destination = -1;
    for (int i = 0; i < (int) preparedSlots.size(); ++i) {
        if (i != published && i != activePrepared) {
            destination = i;
            break;
        }
    }
    if (destination < 0) {
        return false;
    }

    preparedSlots[(size_t) destination] = {
            std::move(prepared), request.generation, request.noteSerial
    };
    preparedGeneration.store(request.generation, std::memory_order_release);
    publishedSlot.store(destination, std::memory_order_release);
    ++preparationCount;
    return true;
}

EnvelopeSignalProcessor::DynamicDiagnostics EnvelopeSignalProcessor::dynamicDiagnostics() const {
    return {
            requestCount.load(std::memory_order_relaxed),
            preparationCount.load(std::memory_order_relaxed),
            adoptionCount.load(std::memory_order_relaxed),
            staleResultCount.load(std::memory_order_relaxed)
    };
}

const EnvelopeConfiguration* EnvelopeSignalProcessor::preparedConfiguration() const {
    const int slot = activeSlot.load(std::memory_order_acquire);
    return slot >= 0
            ? preparedSlots[(size_t) slot].configuration.get()
            : activeConfiguration.get();
}

void EnvelopeSignalProcessor::requestEffectiveMorph(AudioProcessContext& context) {
    const SignalPayload* redInput = inputAt(context, 0);
    const SignalPayload* blueInput = inputAt(context, 1);
    if ((redInput == nullptr || redInput->block.samples.empty())
            && (blueInput == nullptr || blueInput->block.samples.empty())) {
        return;
    }

    const EnvelopeConfiguration* current = preparedConfiguration();
    if (current == nullptr || configuration == nullptr) {
        return;
    }
    const bool noteStarts = std::any_of(
            processVoice(context).events.begin(),
            processVoice(context).events.end(),
            [&](const auto& event) {
                return event.voiceIndex == processVoice(context).voiceIndex
                        && event.type == NoteLifecycleType::NoteOn;
            });
    if (!configuration->dynamicWhileLive && active && !noteStarts) {
        return;
    }

    const float red = redInput != nullptr && !redInput->block.samples.empty()
            ? jlimit(0.f, 1.f, redInput->block.samples.front())
            : configuration->redMorph;
    const float blue = blueInput != nullptr && !blueInput->block.samples.empty()
            ? jlimit(0.f, 1.f, blueInput->block.samples.front())
            : configuration->blueMorph;
    if (hasRequestedMorph && red == lastRequestedRed && blue == lastRequestedBlue) {
        return;
    }

    lastRequestedRed = red;
    lastRequestedBlue = blue;
    hasRequestedMorph = true;
    const uint64_t previous = requestedGeneration.load(std::memory_order_relaxed);
    const uint64_t next = (previous & ~uint64_t(1)) + 2;
    requestedGeneration.store(next - 1, std::memory_order_release);
    requestedRed.store(red, std::memory_order_relaxed);
    requestedBlue.store(blue, std::memory_order_relaxed);
    requestNoteSerial.store(noteStarts ? noteSerial + 1 : noteSerial, std::memory_order_relaxed);
    requestedGeneration.store(next, std::memory_order_release);
    ++requestCount;
}

void EnvelopeSignalProcessor::adoptPreparedDynamicEnvelope() {
    const int slotIndex = publishedSlot.load(std::memory_order_acquire);
    if (slotIndex < 0) {
        return;
    }
    const auto& slot = preparedSlots[(size_t) slotIndex];
    if (slot.generation <= adoptedDynamicGeneration.load(std::memory_order_relaxed)
            || slot.configuration == nullptr) {
        return;
    }

    const bool dynamic = configuration != nullptr && configuration->dynamicWhileLive;
    if (!dynamic && slot.noteSerial != noteSerial) {
        adoptedDynamicGeneration.store(slot.generation, std::memory_order_relaxed);
        ++staleResultCount;
        return;
    }

    activeSlot.store(slotIndex, std::memory_order_release);
    adoptedDynamicGeneration.store(slot.generation, std::memory_order_relaxed);
    playback.validate(slot.configuration->rasterizer->preparedPlaybackView());
    ++adoptionCount;
}

void EnvelopeSignalProcessor::process(AudioProcessContext& context) {
    adoptPreparedDynamicEnvelope();
    requestEffectiveMorph(context);
    auto output = makeOutputPayload(context, 0);
    Buffer<float> outputBuffer = payloadBuffer(output, context.frameCount);
    outputBuffer.zero();

    const bool ready = configuration != nullptr
            ? preparedConfiguration() != nullptr && modelReady
            : syncModel(processParameters(context));
    if (ready) {
        size_t rendered = 0;

        const auto& voice = processVoice(context);
        for (const auto& event : voice.events) {
            if (event.voiceIndex != voice.voiceIndex) {
                continue;
            }

            const size_t eventOffset = std::min(event.sampleOffset, context.frameCount);
            if (eventOffset < rendered) {
                continue;
            }

            renderSegment(outputBuffer, rendered, eventOffset - rendered, context.timing);
            applyLifecycleEvent(event);
            rendered = eventOffset;
        }

        renderSegment(outputBuffer, rendered, context.frameCount - rendered, context.timing);
    }

    outputBuffer.mul(configuration != nullptr ? level : parameterFloat(processParameters(context), "level", 1.f));
    if (context.captureTraversalGrid) {
        publishTraversalGrid(output, context.workArena);
    }
    publishSingleOutput(context, std::move(output));
}

void EnvelopeSignalProcessor::publishTraversalGrid(
        SignalPayload& output,
        const AudioProcessWorkArena* arena) {
    if (output.block.samples.empty()) {
        output.traversalGrid = {};
        return;
    }

    const size_t columns = std::max(defaultTraversalColumns, output.block.samples.size());
    traversalMemory.ensureSize((int) (2 * columns));
    Buffer<float> positions = traversalMemory.place((int) columns);
    Buffer<float> values = traversalMemory.place((int) columns);
    positions.ramp(0.f, 1.f / (float) columns);
    const EnvelopeConfiguration* current = preparedConfiguration();
    const auto sampler = current != nullptr
            ? current->rasterizer->sampler()
            : rasterizer.sampler();
    sampler.sampleAtIntervals(positions, values);
    values.mul(level);

    configureTraversalGrid(
            output.traversalGrid,
            columns,
            output.block.samples.size(),
            makeTraversalGridMetadata(
                    output.domain,
                    columns,
                    output.block.samples.size(),
                    TraversalGridAxis::Time,
                    TraversalGridAxis::Repeated),
            arena);

    const int rows = (int) output.traversalGrid.rows;
    for (size_t column = 0; column < columns; ++column) {
        Buffer<float>(
                output.traversalGrid.values.data() + column * output.traversalGrid.rows,
                rows).set(values[(int) column]);
    }
}

bool EnvelopeSignalProcessor::syncModel(const std::vector<NodeParameter>& parameters) {
    const String nextSnapshot = CurveNodeModelCodec::envelopePayloadFromParameters(parameters);

    const bool snapshotChanged = nextSnapshot != snapshotState;
    if (snapshotChanged) {
        if (!EnvelopeMeshState::apply(nextSnapshot, mesh)) {
            modelReady = false;
            snapshotState = nextSnapshot;
            return false;
        }

        snapshotState = nextSnapshot;
        rasterizer.setMesh(&mesh);
        modelReady = true;
    }

    const float nextRed = parameterFloat(parameters, "red", 0.5f);
    const float nextBlue = parameterFloat(parameters, "blue", 0.5f);
    const bool geometryChanged = nextRed != redMorph || nextBlue != blueMorph;

    if (modelReady && (geometryChanged || snapshotChanged)) {
        redMorph = nextRed;
        blueMorph = nextBlue;
        rasterizer.setMorphPosition({ 0.f, redMorph, blueMorph });
        rasterizer.renderWaveformOnly(&mesh, 0.f);
        rasterizer.validateState();
    }

    props.logarithmic = parameterFloat(parameters, "logarithmic", 0.f) != 0.f;
    level = parameterFloat(parameters, "level", 1.f);
    return modelReady && rasterizer.canRasterizeWaveform();
}

void EnvelopeSignalProcessor::applyLifecycleEvent(const NoteLifecycleEvent& event) {
    if (const EnvelopeConfiguration* current = preparedConfiguration()) {
        const auto prepared = current->rasterizer->preparedPlaybackView();
        switch (event.type) {
            case NoteLifecycleType::NoteOn:
                ++noteSerial;
                playback.noteOn();
                active = true;
                break;

            case NoteLifecycleType::NoteOff:
                playback.noteOff(prepared);
                break;

            case NoteLifecycleType::Reset:
                playback.noteOn();
                active = false;
                break;
        }
        return;
    }

    switch (event.type) {
        case NoteLifecycleType::NoteOn:
            rasterizer.setNoteOn();
            active = true;
            break;

        case NoteLifecycleType::NoteOff:
            rasterizer.setNoteOff();
            break;

        case NoteLifecycleType::Reset:
            rasterizer.reset();
            rasterizer.renderWaveformOnly(&mesh, 0.f);
            active = false;
            break;
    }
}

void EnvelopeSignalProcessor::renderSegment(
        Buffer<float> output,
        size_t start,
        size_t count,
        const AudioProcessTiming& timing) {
    if (!active || count == 0 || timing.sampleRate <= 0.) {
        return;
    }

    bool stillActive {};
    Buffer<float> rendered;
    if (const EnvelopeConfiguration* current = preparedConfiguration()) {
        stillActive = playback.renderToBuffer(
                current->rasterizer->preparedPlaybackView(),
                (int) count,
                1. / timing.sampleRate,
                Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex,
                props,
                1.f);
        rendered = playback.output().withSize((int) count);
    } else {
        stillActive = rasterizer.renderToBuffer(
                (int) count,
                1. / timing.sampleRate,
                EnvRasterizer::headUnisonIndex,
                props,
                1.f);
        rendered = rasterizer.getRenderBuffer().withSize((int) count);
    }
    rendered.copyTo(output.section((int) start, (int) count));
    active = stillActive;
}

}
