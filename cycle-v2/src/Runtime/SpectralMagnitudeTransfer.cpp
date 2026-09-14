#include "Runtime/SpectralMagnitudeTransfer.h"

#include "Graph/GraphCompiler.h"
#include "Runtime/NodeDspConfiguration.h"

#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"

#include <Util/Arithmetic.h>

namespace CycleV2 {

SpectralMagnitudeTransfer resolveSpectralMagnitudeTransfer(
        const GraphExecutionPlan& plan,
        const CompiledSpectralMagnitudeTransfer& compiled) {
    SpectralMagnitudeTransfer transfer;
    if (!compiled.isActive()
            || compiled.sourceStepIndex >= (int) plan.steps.size()) {
        return transfer;
    }

    const auto configuration = std::dynamic_pointer_cast<const TrimeshConfiguration>(
            plan.steps[(size_t) compiled.sourceStepIndex].configuration.value);
    if (configuration == nullptr) {
        return transfer;
    }

    transfer.mode = compiled.mode;
    transfer.range = configuration->range;
    transfer.enabled = configuration->enabled;
    for (const int panStepIndex : compiled.panStepIndices) {
        if (panStepIndex < 0 || panStepIndex >= (int) plan.steps.size()) {
            continue;
        }
        const auto panConfiguration = std::dynamic_pointer_cast<const PanConfiguration>(
                plan.steps[(size_t) panStepIndex].configuration.value);
        if (panConfiguration == nullptr) {
            continue;
        }
        float leftPan = 1.f;
        float rightPan = 1.f;
        Arithmetic::getPans(panConfiguration->pan, leftPan, rightPan);
        transfer.leftPan *= leftPan;
        transfer.rightPan *= rightPan;
    }
    return transfer;
}

}
