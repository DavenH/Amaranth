#include "Nodes/Envelope/EnvelopeSignalProcessor.h"

#include <Curve/Rasterization/Policies/Curves/CurvePolicies.h>
#include <Util/Arithmetic.h>

#include "Graph/NodeParameterMap.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"

namespace CycleV2 {

namespace {

std::shared_ptr<const EnvelopeConfiguration> prepareEnvelopeConfiguration(
        const String& name,
        const EnvelopeMesh& meshState,
        float red,
        float blue,
        float level,
        bool logarithmic,
        bool enabled,
        float neutralValue,
        bool lowResolution,
        bool volumePurpose,
        bool declick) {
    auto result = std::make_shared<EnvelopeConfiguration>();
    result->mesh = std::shared_ptr<EnvelopeMesh>(
            new EnvelopeMesh(name + "Mesh"),
            [](EnvelopeMesh* mesh) {
                mesh->destroy();
                delete mesh;
            });

    result->mesh->deepCopy(&meshState);

    result->rasterizer = std::make_shared<EnvRasterizer>(nullptr, name + "Rasterizer");
    result->rasterizer->setMesh(result->mesh.get());
    result->rasterizer->setMorphPosition({ 0.f, red, blue });
    result->rasterizer->setLowresCurves(lowResolution);
    result->rasterizer->setCalcDepthDimensions(false);
    result->rasterizer->renderWaveformOnly(result->mesh.get(), 0.f);
    result->rasterizer->validateState();

    if (!result->rasterizer->canRasterizeWaveform()) {
        return {};
    }

    result->level = level;
    result->redMorph = red;
    result->blueMorph = blue;
    result->logarithmic = logarithmic;
    result->enabled = enabled;
    result->lowResolution = lowResolution;
    result->neutralValue = neutralValue;
    result->volumePurpose = volumePurpose;
    result->declick = declick;

    result->realtimePlan.mesh = result->mesh.get();
    result->realtimePlan.request.morph = MorphPosition(0.f, red, blue);
    result->realtimePlan.request.cyclic = false;
    result->realtimePlan.request.xMinimum = 0.f;
    result->realtimePlan.request.xMaximum = 10.f;
    result->realtimePlan.request.lowResCurves = lowResolution;
    result->realtimePlan.request.calcDepthDimensions = false;
    result->realtimePlan.capacity = Rasterization::envelopeMaterializationCapacity(
            *result->mesh,
            result->realtimePlan.request,
            result->realtimePlan.guideCurveProvider);
    if (!result->realtimePlan.capacity.isSupported()) {
        return {};
    }
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
    Rasterization::TransferTable::values();

    const NodeModelStatePtr modelToUse = model != nullptr
            ? model
            : CurveNodeDomainCodec(NodeKind::Envelope).createDefault();
    const auto typedModel = std::dynamic_pointer_cast<const CurveNodeModelState>(modelToUse);
    const EnvelopeNodeModel* envelope = typedModel != nullptr ? typedModel->envelope() : nullptr;
    if (envelope == nullptr) {
        return {};
    }
    const NodeParameterMap parameterMap(parameters);
    const String purpose = parameterMap.stringValue("purpose", "control");
    const bool volumePurpose = purpose == "volume";
    return prepareEnvelopeConfiguration(
            "CycleV2EnvelopeConfiguration",
            envelope->getMesh(),
            parameterMap.floatValue("red", 0.5f),
            parameterMap.floatValue("blue", 0.5f),
            parameterMap.floatValue("level", 1.f),
            parameterMap.boolValue("logarithmic", false),
            parameterMap.boolValue("enabled", true),
            volumePurpose ? 1.f : (purpose == "pitch" ? 0.5f : 0.f),
            purpose == "pitch" || purpose == "scratch",
            volumePurpose,
            volumePurpose && parameterMap.boolValue("declick", false));
}

void EnvelopeSignalProcessor::prepareExecution(const AudioExecutionSpec& spec) {
    const size_t maximumColumns = std::max(defaultTraversalColumns, spec.maximumFrameCount);
    traversalMemory.ensureSize((int) (2 * maximumColumns));
    const double sampleRate = spec.sampleRate > 0.
            ? spec.sampleRate
            : CycleDsp::VoiceDeclick::legacySampleRate;
    attackDeclick.resize(CycleDsp::VoiceDeclick::attackSampleCount(sampleRate));
    releaseDeclick.resize(CycleDsp::VoiceDeclick::releaseSampleCount(sampleRate));
    CycleDsp::VoiceDeclick::prepareAttack(attackDeclick);
    CycleDsp::VoiceDeclick::prepareRelease(releaseDeclick);

    if (configuration == nullptr || pendingRevision == adoptedRevision) {
        return;
    }

    activeConfiguration = configuration;
    if (!materializer.prepare(configuration->realtimePlan)) {
        activeConfiguration.reset();
        active = false;
        return;
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

const EnvelopeConfiguration* EnvelopeSignalProcessor::preparedConfiguration() const {
    return activeConfiguration.get();
}

Rasterization::PreparedEnvelopePlaybackView EnvelopeSignalProcessor::preparedPlaybackView() const {
    if (materializer.hasPreparedEnvelope()) {
        return materializer.preparedPlaybackView();
    }
    return activeConfiguration->rasterizer->preparedPlaybackView();
}

bool EnvelopeSignalProcessor::prepareNoteEnvelope(
        const AudioProcessContext& context,
        size_t sampleOffset) {
    if (activeConfiguration == nullptr) {
        return false;
    }

    const auto valueAt = [sampleOffset](const SignalPayload* input, float fallback) {
        if (input == nullptr || input->block.samples.empty()) {
            return fallback;
        }
        if (input->block.samples.size() == 1) {
            return jlimit(0.f, 1.f, input->block.samples.front());
        }
        const size_t index = std::min(sampleOffset, input->block.samples.size() - 1);
        return jlimit(0.f, 1.f, input->block.samples[index]);
    };
    const SignalPayload* redInput = inputAt(context, 0);
    const SignalPayload* blueInput = inputAt(context, 1);
    const float red = valueAt(redInput, activeConfiguration->redMorph);
    const float blue = valueAt(blueInput, activeConfiguration->blueMorph);
    return materializer.materialize(red, blue);
}

void EnvelopeSignalProcessor::process(AudioProcessContext& context) {
    auto output = makeOutputPayload(context, 0);
    Buffer<float> outputBuffer = payloadBuffer(output, context.frameCount);
    outputBuffer.zero();

    const EnvelopeConfiguration* current = preparedConfiguration();
    if (current != nullptr && !current->enabled
            && !(current->volumePurpose && current->declick)) {
        outputBuffer.set(current->neutralValue);
        if (context.captureTraversalGrid) {
            publishTraversalGrid(output, context.workArena);
        }
        publishSingleOutput(context, std::move(output));
        return;
    }

    const bool ready = current != nullptr;
    if (ready) {
        size_t rendered = 0;

        const auto& voice = processVoice(context);
        const double sampleRateIncrement = context.timing.sampleRate > 0.
                ? 1. / context.timing.sampleRate
                : 0.;
        const float routedTimeIncrement = current->volumePurpose
                        && voice.controls.normalizedVolumeEnvelopeTimeIncrement > 0.f
                ? voice.controls.normalizedVolumeEnvelopeTimeIncrement
                : voice.controls.normalizedVoiceTimeIncrement;
        const double normalizedTimeIncrement = routedTimeIncrement > 0.f
                ? (double) routedTimeIncrement
                : sampleRateIncrement;
        for (const auto& event : voice.events) {
            if (event.voiceIndex != voice.voiceIndex) {
                continue;
            }

            const size_t eventOffset = std::min(event.sampleOffset, context.frameCount);
            if (eventOffset < rendered) {
                continue;
            }

            renderSegment(outputBuffer, rendered, eventOffset - rendered, normalizedTimeIncrement);
            applyLifecycleEvent(event, context, eventOffset);
            rendered = eventOffset;
        }

        renderSegment(outputBuffer, rendered, context.frameCount - rendered, normalizedTimeIncrement);
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

    if (!current->enabled) {
        values.set(current->neutralValue);
    } else {
        const auto prepared = preparedPlaybackView();
        const auto& result = playback.mode() == Rasterization::EnvelopePlaybackMode::Looping
                && prepared.loop.sampleable
                ? prepared.loop
                : prepared.display;
        const Rasterization::SamplerView sampler(result.waveform, result.sampleable);
        sampler.sampleAtIntervals(positions, values);
        if (current->logarithmic) {
            Arithmetic::applyInvLogMapping(values, 30.f);
        }
        values.mul(level);
    }

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

void EnvelopeSignalProcessor::applyLifecycleEvent(
        const NoteLifecycleEvent& event,
        const AudioProcessContext& context,
        size_t sampleOffset) {
    const EnvelopeConfiguration* current = preparedConfiguration();
    if (current == nullptr) {
        return;
    }

    switch (event.type) {
        case NoteLifecycleType::NoteOn:
            if (!prepareNoteEnvelope(context, sampleOffset)) {
                active = false;
                fadingIn = false;
                fadingOut = false;
                break;
            }
            playback.noteOn();
            active = true;
            fadingIn = current->volumePurpose && current->declick;
            fadingOut = false;
            attackSamplePosition = 0;
            releaseSamplePosition = 0;
            break;

        case NoteLifecycleType::NoteOff:
            if (!current->enabled || !playback.noteOff(preparedPlaybackView())) {
                fadingOut = current->volumePurpose && current->declick;
                releaseSamplePosition = 0;
                active = fadingOut;
            }
            break;

        case NoteLifecycleType::Reset:
            playback.noteOn();
            active = false;
            fadingIn = false;
            fadingOut = false;
            break;
    }
}

void EnvelopeSignalProcessor::renderSegment(
        Buffer<float> output,
        size_t start,
        size_t count,
        double normalizedTimeIncrement) {
    if (!active || count == 0 || normalizedTimeIncrement <= 0.) {
        return;
    }

    const EnvelopeConfiguration* current = preparedConfiguration();
    if (current == nullptr) {
        return;
    }

    if (!current->enabled) {
        renderNeutralSegment(output, start, count);
        return;
    }

    const int releaseSamplesRemaining = current->volumePurpose
            ? playback.releaseSamplesRemaining(
                    preparedPlaybackView(),
                    normalizedTimeIncrement,
                    Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex,
                    props,
                    1.f)
            : -1;
    const bool stillActive = playback.renderToBuffer(
            preparedPlaybackView(),
            (int) count,
            normalizedTimeIncrement,
            Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex,
            props,
            1.f);
    Buffer<float> rendered = playback.output().withSize((int) count);
    const bool applyingReleaseDeclick = fadingOut;
    if (fadingIn) {
        applyAttackDeclick(rendered);
    } else if (fadingOut) {
        renderReleaseDeclick(rendered);
    } else if (current->volumePurpose) {
        CycleDsp::VoiceDeclick::applyReleaseTail(
                rendered,
                releaseDeclick,
                releaseSamplesRemaining);
    }
    rendered.copyTo(output.section((int) start, (int) count));
    active = applyingReleaseDeclick ? fadingOut : stillActive;
}

void EnvelopeSignalProcessor::renderNeutralSegment(
        Buffer<float> output,
        size_t start,
        size_t count) {
    if (!active || count == 0) {
        return;
    }

    Buffer<float> rendered = output.section((int) start, (int) count);
    rendered.set(1.f);
    if (fadingIn) {
        applyAttackDeclick(rendered);
    } else if (fadingOut) {
        renderReleaseDeclick(rendered);
    }
}

void EnvelopeSignalProcessor::applyAttackDeclick(Buffer<float> rendered) {
    const int samples = jmin(
            rendered.size(),
            attackDeclick.size() - attackSamplePosition);
    if (samples > 0) {
        rendered.withSize(samples).mul(
                attackDeclick.section(attackSamplePosition, samples));
        attackSamplePosition += samples;
    }
    fadingIn = attackSamplePosition < attackDeclick.size();
}

void EnvelopeSignalProcessor::renderReleaseDeclick(Buffer<float> rendered) {
    const int samples = jmin(
            rendered.size(),
            releaseDeclick.size() - releaseSamplePosition);
    if (samples > 0) {
        rendered.withSize(samples).mul(
                releaseDeclick.section(releaseSamplePosition, samples));
        releaseSamplePosition += samples;
    }
    if (samples < rendered.size()) {
        rendered.offset(samples).zero();
    }
    fadingOut = releaseSamplePosition < releaseDeclick.size();
    active = fadingOut;
}

}
