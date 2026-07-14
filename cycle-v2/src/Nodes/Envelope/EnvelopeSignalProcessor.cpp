#include "EnvelopeSignalProcessor.h"

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

    const String snapshot = typedParameterString(parameters, EnvelopeMeshState::parameterId());

    if (!EnvelopeMeshState::apply(snapshot, *result->mesh)) {
        return {};
    }

    result->rasterizer = std::make_shared<EnvRasterizer>(nullptr, "CycleV2EnvelopeConfigurationRasterizer");
    result->rasterizer->setMesh(result->mesh.get());
    result->rasterizer->setMorphPosition({
            0.f,
            parameterFloat(parameters, "red", 0.f),
            parameterFloat(parameters, "blue", 0.f)
    });
    result->rasterizer->updateWaveform(result->mesh.get(), 0.f);
    result->rasterizer->validateState();
    if (!result->rasterizer->canRasterizeWaveform()) {
        return {};
    }

    result->level = parameterFloat(parameters, "level", 1.f);
    result->logarithmic = parameterFloat(parameters, "logarithmic", 0.f) != 0.f;
    return result;
}

void EnvelopeSignalProcessor::prepareExecution(const AudioExecutionSpec&) {
    if (configuration == nullptr || pendingRevision == adoptedRevision) {
        return;
    }

    rasterizer.adoptPreparedData(*configuration->rasterizer);
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

void EnvelopeSignalProcessor::process(AudioProcessContext& context) {
    auto output = makeOutputPayload(context, 0);
    Buffer<float> outputBuffer = payloadBuffer(output, context.frameCount);
    outputBuffer.zero();

    const bool ready = configuration != nullptr ? modelReady : syncModel(context.parameters);
    if (ready) {
        size_t rendered = 0;

        for (const auto& event : context.voice.events) {
            if (event.voiceIndex != context.voice.voiceIndex) {
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

    outputBuffer.mul(configuration != nullptr ? level : parameterFloat(context.parameters, "level", 1.f));
    publishVectorAsTraversalGrid(output, defaultTraversalColumns, context.workArena);
    publishSingleOutput(context, std::move(output));
}

bool EnvelopeSignalProcessor::syncModel(const std::vector<NodeParameter>& parameters) {
    const String nextSnapshot = typedParameterString(parameters, EnvelopeMeshState::parameterId());

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

    const float nextRed = parameterFloat(parameters, "red", 0.f);
    const float nextBlue = parameterFloat(parameters, "blue", 0.f);
    const bool geometryChanged = nextRed != redMorph || nextBlue != blueMorph;

    if (modelReady && (geometryChanged || snapshotChanged)) {
        redMorph = nextRed;
        blueMorph = nextBlue;
        rasterizer.setMorphPosition({ 0.f, redMorph, blueMorph });
        rasterizer.updateWaveform(&mesh, 0.f);
        rasterizer.validateState();
    }

    props.logarithmic = parameterFloat(parameters, "logarithmic", 0.f) != 0.f;
    return modelReady && rasterizer.canRasterizeWaveform();
}

void EnvelopeSignalProcessor::applyLifecycleEvent(const NoteLifecycleEvent& event) {
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
            rasterizer.updateWaveform(&mesh, 0.f);
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

    const bool stillActive = rasterizer.renderToBuffer(
            (int) count,
            1. / timing.sampleRate,
            EnvRasterizer::headUnisonIndex,
            props,
            1.f);
    rasterizer.getRenderBuffer().withSize((int) count).copyTo(
            output.section((int) start, (int) count));
    active = stillActive;
}

}
