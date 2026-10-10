#include "Runtime/PreviewMorphBinding.h"

#include "Nodes/Control/ModulationSource.h"
#include "Nodes/Control/ModulationTriple.h"

namespace CycleV2 {

namespace {

const ModulationSourceConfiguration* sourceFor(
        const GraphExecutionPlan& plan,
        const GraphStepInput& input) {
    if (isPositiveAndBelow(input.sourceBufferIndex, (int) plan.buffers.size())) {
        const auto& buffer = plan.buffers[(size_t) input.sourceBufferIndex];
        const int slot = (int) buffer.defaultModulationSlot - 1;
        const auto triple = std::dynamic_pointer_cast<
                const ModulationTripleConfiguration>(buffer.defaultModulation);
        if (triple != nullptr && isPositiveAndBelow(slot, (int) triple->sources.size())) {
            return &triple->sources[(size_t) slot];
        }
    }
    if (!isPositiveAndBelow(input.sourceStepIndex, (int) plan.steps.size())) {
        return nullptr;
    }
    const auto& sourceStep = plan.steps[(size_t) input.sourceStepIndex];
    const auto direct = std::dynamic_pointer_cast<
            const ModulationSourceConfiguration>(sourceStep.configuration.value);
    if (direct != nullptr) {
        return direct.get();
    }
    const auto triple = std::dynamic_pointer_cast<
            const ModulationTripleConfiguration>(sourceStep.configuration.value);
    return triple != nullptr
            && isPositiveAndBelow(input.sourceOutputIndex, (int) triple->sources.size())
            ? &triple->sources[(size_t) input.sourceOutputIndex]
            : nullptr;
}

bool isMorphAxis(const String& portId) {
    return portId == "yellow" || portId == "red" || portId == "blue";
}

}

std::vector<PreviewMorphTarget> PreviewMorphBinding::fromPlan(
        const GraphExecutionPlan& plan) {
    std::vector<PreviewMorphTarget> targets;
    // Preserve step order so the dispatcher can apply both Envelope axes together.
    for (const auto& step : plan.steps) {
        if (step.kind != NodeKind::TrilinearMesh && step.kind != NodeKind::Envelope) {
            continue;
        }
        for (const auto& input : step.inputs) {
            if (!isMorphAxis(input.destPortId)) {
                continue;
            }
            const auto* source = sourceFor(plan, input);
            if (source == nullptr) {
                continue;
            }
            if (source->mode == ModulationSourceMode::KeyScale) {
                targets.push_back({ step.nodeId, input.destPortId,
                        PreviewMorphControl::KeyScale });
            } else if (source->mode == ModulationSourceMode::ModWheel
                    || (source->mode == ModulationSourceMode::MidiController
                            && source->controller == 1)) {
                targets.push_back({ step.nodeId, input.destPortId,
                        PreviewMorphControl::ModWheel });
            }
        }
    }
    return targets;
}

}
