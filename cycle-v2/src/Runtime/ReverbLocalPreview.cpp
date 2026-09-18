#include "Runtime/ReverbLocalPreview.h"

namespace CycleV2 {

std::optional<ReverbLocalPreviewInput> ReverbLocalPreview::prepare(
        const Node& node,
        const GraphExecutionStep& step) {
    if (step.previewRole != PreviewModuleRole::ReverbSpectrogram) {
        return std::nullopt;
    }

    ReverbLocalPreviewInput input;
    input.nodeId = node.id;
    input.parameters = node.parameters;
    const auto previous = std::dynamic_pointer_cast<const ReverbConfiguration>(
            step.configuration.value);
    if (previous != nullptr) {
        input.previousKernel = previous->kernel;
    }
    return input;
}

NodePreviewResult ReverbLocalPreview::render(const ReverbLocalPreviewInput& input) {
    ReverbConfiguration previous;
    previous.kernel = input.previousKernel;
    const auto configuration = ReverbSignalProcessor::buildConfiguration(
            input.parameters, &previous);
    const PublishedNodeConfiguration localConfiguration {
            1, {}, configuration
    };
    PreviewProcessContext context;
    context.pointCount = 40;
    context.parameters = input.parameters;
    context.configuration = &localConfiguration;
    auto processor = NodePreviewProcessorFactory().create(
            PreviewModuleRole::ReverbSpectrogram);
    processor->render(context);
    return {
            input.nodeId,
            PreviewModuleRole::ReverbSpectrogram,
            std::move(context.primary),
            std::move(context.secondary),
            context.gridColumns,
            context.gridRows,
            context.domain,
            context.frequencySampling,
            context.frequencyMidiNote
    };
}

}
