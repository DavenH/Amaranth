#include "Runtime/PreparedTrimeshMorphBinding.h"

#include "Nodes/Control/ModulationSource.h"
#include "Nodes/Control/ModulationTriple.h"

namespace CycleV2 {

namespace {

bool validSignalBuffer(const GraphExecutionPlan& plan, int bufferIndex) {
    return bufferIndex >= 0 && bufferIndex < (int) plan.buffers.size();
}

int morphIndexFor(const String& portId) {
    return portId == "yellow" ? 0
            : portId == "red" ? 1
            : portId == "blue" ? 2
            : -1;
}

bool usesVoiceTimeSource(
        const GraphExecutionPlan& plan,
        const GraphStepInput& input) {
    if (validSignalBuffer(plan, input.sourceBufferIndex)) {
        const auto& buffer = plan.buffers[(size_t) input.sourceBufferIndex];
        const auto defaultConfiguration = std::dynamic_pointer_cast<
                const ModulationTripleConfiguration>(buffer.defaultModulation);
        const int sourceIndex = (int) buffer.defaultModulationSlot - 1;
        if (defaultConfiguration != nullptr
                && sourceIndex >= 0
                && sourceIndex < (int) defaultConfiguration->sources.size()
                && defaultConfiguration->sources[(size_t) sourceIndex].mode
                        == ModulationSourceMode::VoiceTime) {
            return true;
        }
    }
    if (input.sourceStepIndex < 0
            || input.sourceStepIndex >= (int) plan.steps.size()) {
        return false;
    }
    const auto sourceConfiguration = std::dynamic_pointer_cast<
            const ModulationSourceConfiguration>(
                    plan.steps[(size_t) input.sourceStepIndex].configuration.value);
    return sourceConfiguration != nullptr
            && sourceConfiguration->mode == ModulationSourceMode::VoiceTime;
}

}

bool PreparedTrimeshMorphBinding::supports(
        const GraphExecutionPlan& plan,
        const GraphExecutionStep& step) {
    for (const auto& input : step.inputs) {
        if (morphIndexFor(input.destPortId) >= 0
                && !validSignalBuffer(plan, input.sourceBufferIndex)) {
            return false;
        }
    }
    for (const auto& attachment : step.attachments) {
        if (attachment.destPortId == "scratch"
                && !validSignalBuffer(plan, attachment.sourceBufferIndex)) {
            return false;
        }
    }
    return true;
}

void PreparedTrimeshMorphBinding::bind(
        const GraphExecutionPlan& plan,
        const GraphExecutionStep& step) {
    for (const auto& input : step.inputs) {
        const int morphIndex = morphIndexFor(input.destPortId);
        if (morphIndex < 0) {
            continue;
        }
        morphInputBuffers[(size_t) morphIndex] = input.sourceBufferIndex;
        voiceTimeMorph[(size_t) morphIndex] = usesVoiceTimeSource(plan, input);
    }
    for (const auto& attachment : step.attachments) {
        if (attachment.destPortId == "scratch") {
            scratchBuffer = attachment.sourceBufferIndex;
        }
    }
}

TrimeshMorphInputs PreparedTrimeshMorphBinding::inputsFor(
        const PreparedOscillatorProcessContext& context,
        size_t blockSampleOffset,
        double voiceSamplePosition) const {
    TrimeshMorphInputs inputs;
    for (size_t axis = 0; axis < inputs.absoluteMorph.size(); ++axis) {
        inputs.absoluteMorph[axis] = context.signalAt(morphInputBuffers[axis]);
        if (!voiceTimeMorph[axis]
                || context.voice == nullptr
                || context.voice->controls.normalizedVoiceTimeIncrement <= 0.f) {
            continue;
        }
        inputs.absoluteOverrides[axis] = (float) (
                voiceSamplePosition
                * context.voice->controls.normalizedVoiceTimeIncrement);
        inputs.hasAbsoluteOverride[axis] = true;
    }
    inputs.scratch = context.signalAt(scratchBuffer);
    return inputs;
}

}
