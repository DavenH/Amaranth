#include "EnvelopeSignalProcessor.h"

#include "../Effect2D/CurveNodeModels.h"

#include <cmath>

namespace CycleV2 {

namespace {

std::shared_ptr<const EnvelopeConfiguration> prepareEnvelopeConfiguration(
        const String& name,
        const var& meshState,
        float red,
        float blue,
        float level,
        bool logarithmic,
        bool dynamicWhileLive) {
    auto result = std::make_shared<EnvelopeConfiguration>();
    result->mesh = std::shared_ptr<EnvelopeMesh>(
            new EnvelopeMesh(name + "Mesh"),
            [](EnvelopeMesh* mesh) {
                mesh->destroy();
                delete mesh;
            });

    if (!result->mesh->readJSON(meshState)) {
        return {};
    }

    result->rasterizer = std::make_shared<EnvRasterizer>(nullptr, name + "Rasterizer");
    result->rasterizer->setMesh(result->mesh.get());
    result->rasterizer->setMorphPosition({ 0.f, red, blue });
    result->rasterizer->renderWaveformOnly(result->mesh.get(), 0.f);
    result->rasterizer->validateState();

    if (!result->rasterizer->canRasterizeWaveform()) {
        return {};
    }

    result->level = level;
    result->redMorph = red;
    result->blueMorph = blue;
    result->logarithmic = logarithmic;
    result->dynamicWhileLive = dynamicWhileLive;
    return result;
}

}

EnvelopeSignalProcessor::EnvelopeSignalProcessor() {
    props.active = true;
}

std::shared_ptr<const EnvelopeConfiguration> EnvelopeSignalProcessor::buildConfiguration(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model) {
    if (Curve::table == nullptr) {
        Curve::calcTable();
    }

    const NodeModelStatePtr modelToUse = model != nullptr
            ? model
            : CurveNodeDomainCodec(NodeKind::Envelope).createDefault();
    const auto typedModel = std::dynamic_pointer_cast<const CurveNodeModelState>(modelToUse);
    EnvelopeNodeModel envelope;
    if (typedModel == nullptr || !envelope.readJSON(typedModel->domainJSON())) {
        return {};
    }
    return prepareEnvelopeConfiguration(
            "CycleV2EnvelopeConfiguration",
            envelope.getMesh().writeJSON(),
            parameterFloat(parameters, "red", 0.5f),
            parameterFloat(parameters, "blue", 0.5f),
            parameterFloat(parameters, "level", 1.f),
            parameterFloat(parameters, "logarithmic", 0.f) != 0.f,
            typedParameterBool(parameters, "dynamic", false));
}

void EnvelopeSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    const size_t maximumColumns = std::max(defaultTraversalColumns, spec.maximumFrameCount);
    traversalMemory.ensureSize((int) (2 * maximumColumns));
    transitionMemory.ensureSize((int) spec.maximumFrameCount);

    if (configuration == nullptr || pendingRevision == adoptedRevision) {
        return;
    }

    const EnvelopeConfiguration* previous = preparedConfiguration();
    const bool freezeCurrentMorph = active
            && previous != nullptr
            && previous->dynamicWhileLive
            && !configuration->dynamicWhileLive;
    if (freezeCurrentMorph) {
        activeConfiguration = preparedEnvelopes.activeOwnership(activeConfiguration);
    } else {
        activeConfiguration = configuration;
    }
    preparationRequests.reset();
    preparedEnvelopes.reset();
    hasRequestedMorph = false;
    const MorphPosition baseMorph(0.f, configuration->redMorph, configuration->blueMorph);
    if (!morphInitialized) {
        smoothedMorph.reset(baseMorph);
        morphInitialized = true;
    } else {
        smoothedMorph.setTargets(baseMorph);
    }
    playback.validate(activeConfiguration->rasterizer->preparedPlaybackView());
    props.logarithmic = configuration->logarithmic;
    level = configuration->level;
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
    return prepareEnvelopeConfiguration(
            "CycleV2DynamicEnvelope",
            base.mesh->writeJSON(),
            red,
            blue,
            base.level,
            base.logarithmic,
            base.dynamicWhileLive);
}

bool EnvelopeSignalProcessor::serviceNonRealtimePreparation() {
    EnvelopePreparationRequest request;
    if (configuration == nullptr
            || !preparationRequests.latest(request)) {
        return false;
    }

    auto prepared = prepareDynamicConfiguration(
            *configuration, request.red, request.blue);
    if (prepared == nullptr) {
        return false;
    }
    if (!preparationRequests.isCurrent(request.generation)) {
        preparationRequests.recordStaleResult();
        return false;
    }
    if (!preparedEnvelopes.publish(
            std::move(prepared), request.generation, request.noteSerial)) {
        return false;
    }
    preparationRequests.markPrepared(request.generation);
    return true;
}

EnvelopeSignalProcessor::DynamicDiagnostics EnvelopeSignalProcessor::dynamicDiagnostics() const {
    return {
            preparationRequests.publicationCount(),
            preparedEnvelopes.preparationCount(),
            preparedEnvelopes.adoptionCount(),
            preparationRequests.staleResultCount() + preparedEnvelopes.staleResultCount()
    };
}

const EnvelopeConfiguration* EnvelopeSignalProcessor::preparedConfiguration() const {
    return preparedEnvelopes.active(activeConfiguration.get());
}

void EnvelopeSignalProcessor::requestEffectiveMorph(AudioProcessContext& context) {
    const SignalPayload* redInput = inputAt(context, 0);
    const SignalPayload* blueInput = inputAt(context, 1);
    const bool hasRedInput = redInput != nullptr && !redInput->block.samples.empty();
    const bool hasBlueInput = blueInput != nullptr && !blueInput->block.samples.empty();
    if (!hasRedInput && !hasBlueInput) {
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
    const float redTarget = hasRedInput
            ? jlimit(0.f, 1.f, redInput->block.samples.front())
            : configuration->redMorph;
    const float blueTarget = hasBlueInput
            ? jlimit(0.f, 1.f, blueInput->block.samples.front())
            : configuration->blueMorph;
    smoothedMorph.setTargets({ 0.f, redTarget, blueTarget });
    samplesSinceMorphRequest += smoothedMorph.advance(
            context.frameCount,
            context.timing.sampleRate);

    if (!configuration->dynamicWhileLive && active && !noteStarts) {
        return;
    }

    const float red = smoothedMorph.current().red.getCurrentValue();
    const float blue = smoothedMorph.current().blue.getCurrentValue();
    const bool reachedTarget = red == smoothedMorph.current().red.getTargetValue()
            && blue == smoothedMorph.current().blue.getTargetValue();
    const bool changedMeaningfully = !hasRequestedMorph
            || std::abs(red - lastRequestedRed) >= morphRequestThreshold
            || std::abs(blue - lastRequestedBlue) >= morphRequestThreshold
            || (reachedTarget && (red != lastRequestedRed || blue != lastRequestedBlue));
    if (!changedMeaningfully
            || (!noteStarts && samplesSinceMorphRequest < morphRequestInterval44k)) {
        return;
    }

    lastRequestedRed = red;
    lastRequestedBlue = blue;
    hasRequestedMorph = true;
    samplesSinceMorphRequest = 0;
    preparationRequests.publish(
            red,
            blue,
            noteStarts ? noteSerial + 1 : noteSerial);
}

void EnvelopeSignalProcessor::adoptPreparedDynamicEnvelope() {
    const bool dynamic = configuration != nullptr && configuration->dynamicWhileLive;
    const auto adoption = preparedEnvelopes.adoptNewest(noteSerial, dynamic);
    if (!adoption.adopted || adoption.configuration == nullptr) {
        return;
    }
    playback.validate(adoption.configuration->rasterizer->preparedPlaybackView());
    adoptionTransitionPending = active;
}

void EnvelopeSignalProcessor::process(AudioProcessContext& context) {
    adoptPreparedDynamicEnvelope();
    requestEffectiveMorph(context);
    auto output = makeOutputPayload(context, 0);
    Buffer<float> outputBuffer = payloadBuffer(output, context.frameCount);
    outputBuffer.zero();

    const bool ready = preparedConfiguration() != nullptr;
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

    outputBuffer.mul(level);
    if (context.captureTraversalGrid) {
        publishTraversalGrid(output, context.workArena);
    }
    publishSingleOutput(context, std::move(output));
}

void EnvelopeSignalProcessor::publishTraversalGrid(
        SignalPayload& output,
        const AudioProcessWorkArena* arena) {
    if (output.block.samples.empty()) {
        clearTraversalGrid(output.traversalGrid);
        return;
    }

    const size_t columns = std::max(defaultTraversalColumns, output.block.samples.size());
    traversalMemory.ensureSize((int) (2 * columns));
    Buffer<float> positions = traversalMemory.place((int) columns);
    Buffer<float> values = traversalMemory.place((int) columns);
    positions.ramp(0.f, 1.f / (float) columns);
    const EnvelopeConfiguration* current = preparedConfiguration();
    if (current == nullptr) {
        clearTraversalGrid(output.traversalGrid);
        return;
    }

    const auto sampler = current->rasterizer->sampler();
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

void EnvelopeSignalProcessor::applyLifecycleEvent(const NoteLifecycleEvent& event) {
    const EnvelopeConfiguration* current = preparedConfiguration();
    if (current == nullptr) {
        return;
    }

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
}

void EnvelopeSignalProcessor::renderSegment(
        Buffer<float> output,
        size_t start,
        size_t count,
        const AudioProcessTiming& timing) {
    if (!active || count == 0 || timing.sampleRate <= 0.) {
        return;
    }

    const EnvelopeConfiguration* current = preparedConfiguration();
    if (current == nullptr) {
        return;
    }

    const bool stillActive = playback.renderToBuffer(
            current->rasterizer->preparedPlaybackView(),
            (int) count,
            1. / timing.sampleRate,
            Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex,
            props,
            1.f);
    Buffer<float> rendered = playback.output().withSize((int) count);
    applyAdoptionTransition(rendered);
    rendered.copyTo(output.section((int) start, (int) count));
    lastOutputSample = rendered.back();
    active = stillActive;
}

void EnvelopeSignalProcessor::applyAdoptionTransition(Buffer<float> rendered) {
    if (rendered.empty()) {
        return;
    }
    if (adoptionTransitionPending) {
        transitionOffset = lastOutputSample - rendered.front();
        transitionSamplesRemaining = 128;
        adoptionTransitionPending = false;
    }
    if (transitionSamplesRemaining <= 0) {
        return;
    }

    const int count = std::min(rendered.size(), transitionSamplesRemaining);
    const float decrement = transitionOffset / 128.f;
    const float start = decrement * (float) transitionSamplesRemaining;
    Buffer<float> correction = transitionMemory.withSize(count);
    correction.ramp(start, -decrement);
    rendered.withSize(count).add(correction);
    transitionSamplesRemaining -= count;
}

}
