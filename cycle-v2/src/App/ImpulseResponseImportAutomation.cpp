#include <utility>

#include "App/ImpulseResponseImportAutomation.h"

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/ImpulseResponse/ImpulseResponseResourcePreparation.h"
#include "Nodes/ImpulseResponse/ImpulseResponseAnalysis.h"

namespace CycleV2 {

namespace {

var importedState(const GraphDocument& document, const String& nodeId, const File& file,
        int impulseLength) {
    const Node& node = *document.graph().findNode(nodeId);
    const auto& binding = *document.graph().findAudioResourceBinding(nodeId);
    const auto& resource = *document.graph().findAudioResource(binding.resourceId);
    const auto model = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    auto result = var(new DynamicObject());
    auto& fields = *result.getDynamicObject();
    fields.setProperty("nodeId", nodeId);
    fields.setProperty("sourcePath", file.getFullPathName());
    fields.setProperty("resourceId", resource.id);
    fields.setProperty("sourceSampleRate", resource.sampleRate);
    fields.setProperty("sampleRatePolicy", impulseResponseSampleRatePolicy);
    fields.setProperty("canonicalSampleRate", impulseResponseReferenceSampleRate);
    fields.setProperty("resourceSampleCount", (int) resource.samples.size());
    fields.setProperty("sourceNormalization", "PitchedSample peak-normalized audio; first channel retained");
    fields.setProperty("mode", binding.mode);
    fields.setProperty("impulseLength", impulseLength);
    fields.setProperty("modelVertexCount", model != nullptr && model->flatCurve() != nullptr
            ? (int) model->flatCurve()->getVertices().size() : 0);
    fields.setProperty("modelRevision", node.model != nullptr ? (int64) node.model->revision() : 0);
    fields.setProperty("documentRevision", (int64) document.revision());
    auto parameters = var(new DynamicObject());
    for (const auto& parameter : node.parameters) {
        parameters.getDynamicObject()->setProperty(parameter.id, parameter.value);
    }
    fields.setProperty("parameters", parameters);
    return result;
}

}

var ImpulseResponseImportAutomation::apply(
        GraphDocument& document,
        GraphCommandDispatcher& commands,
        const var& request,
        const File& file,
        String& error) {
    error.clear();
    const String nodeId = request["nodeId"].toString();
    const String mode = request["mode"].toString();
    const Node* node = document.graph().findNode(nodeId);
    if (!request["nodeId"].isString() || nodeId.isEmpty() || !request["mode"].isString()
            || !request["path"].isString() || request["path"].toString().isEmpty()
            || (mode != "direct" && mode != "modelled")) {
        error = "Requires nodeId, path and explicit mode direct or modelled";
        return {};
    }
    if (commands.hasTransientEdit() || node == nullptr || node->kind != NodeKind::ImpulseResponse
            || !file.existsAsFile()) {
        error = commands.hasTransientEdit() ? "Cannot import an impulse response during a gesture"
                : "Requires an Impulse Response node and an existing audio file";
        return {};
    }

    PreparedImpulseResponseAudio prepared;
    const Result preparation = ImpulseResponseResourcePreparation::prepare(file,
            mode == "direct" ? ImpulseResponseImportMode::Direct : ImpulseResponseImportMode::Modelled,
            *node, prepared);
    if (preparation.failed()) {
        error = preparation.getErrorMessage();
        return {};
    }
    const GraphEditResult edit = commands.setNodeAudioResource(std::move(prepared.edit));
    if (!edit.succeeded()) {
        error = edit.message.isNotEmpty() ? edit.message : "The prepared impulse response could not be applied";
        return {};
    }
    return importedState(document, nodeId, file, prepared.impulseLength);
}

}
