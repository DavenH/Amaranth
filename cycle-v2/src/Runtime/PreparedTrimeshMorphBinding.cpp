#include "Runtime/PreparedTrimeshMorphBinding.h"

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

void PreparedTrimeshMorphBinding::bind(const GraphExecutionStep& step) {
    for (const auto& input : step.inputs) {
        const int morphIndex = morphIndexFor(input.destPortId);
        if (morphIndex < 0) {
            continue;
        }
        morphInputBuffers[(size_t) morphIndex] = input.sourceBufferIndex;
    }
    for (const auto& attachment : step.attachments) {
        if (attachment.destPortId == "scratch") {
            scratchBuffer = attachment.sourceBufferIndex;
        }
    }
}

TrimeshMorphInputs PreparedTrimeshMorphBinding::inputsFor(
        const PreparedOscillatorProcessContext& context) const {
    TrimeshMorphInputs inputs;
    for (size_t axis = 0; axis < inputs.absoluteMorph.size(); ++axis) {
        inputs.absoluteMorph[axis] = context.signalAt(morphInputBuffers[axis]);
    }
    inputs.scratch = context.signalAt(scratchBuffer);
    return inputs;
}

}
