#include <cmath>
#include <limits>

#include "App/CycleV2Automation.h"

#include "App/CycleV2AutomationProtocol.h"
#include "App/CycleV2AutomationCommand.h"
#include "Graph/GraphEditTypes.h"
#include "Nodes/ModelAuthoringValidation.h"
#include "Graph/SynthesisCapabilities.h"

namespace CycleV2 {

using namespace AutomationProtocol;

namespace {

bool numeric(const var& value) {
    return value.isInt() || value.isInt64() || value.isDouble();
}

bool readContext(const var& command, ParameterMappingContext& context) {
    const auto sampleRate = command.getProperty("sampleRate", context.sampleRate);
    const auto beats = command.getProperty("beatsPerMeasure", context.beatsPerMeasure);
    if (!numeric(sampleRate) || !numeric(beats)
            || !std::isfinite((double) beats) || (double) beats < 1.0
            || (double) beats > (double) std::numeric_limits<int>::max()
            || std::floor((double) beats) != (double) beats) {
        return false;
    }
    context.sampleRate = (double) sampleRate;
    context.beatsPerMeasure = (int) beats;
    return context.isValid();
}

}

var CycleV2Automation::inspectSynthesisCapabilities(const var& command) const {
    ParameterMappingContext context;
    if (!readContext(command, context)) {
        return failedResult("inspectSynthesisCapabilities", "Invalid mapping context");
    }
    var capabilities = SynthesisCapabilities::exportJSON(context);
    var authoring = makeObject();
    auto& fields = *objectFor(authoring);
    fields.setProperty("command", automationCommandName(CycleV2AutomationCommand::ApplyNodeEdits));
    fields.setProperty("maximumNodeEdits", (int) maximumNodeAuthoringEdits);
    fields.setProperty("maximumParameters", (int) maximumNodeAuthoringParameters);
    fields.setProperty("maximumModelVertices", ModelAuthoringValidation::maximumVertices);
    fields.setProperty("maximumModelCubes", ModelAuthoringValidation::maximumCubes);
    fields.setProperty("atomic", true);
    fields.setProperty("requiresDocumentRevision", true);
    fields.setProperty("requiresModelRevisionForReplacement", true);
    Array<var> lifecycle;
    for (const auto operation : { CycleV2AutomationCommand::InspectNodeControls,
            CycleV2AutomationCommand::OpenGraph, CycleV2AutomationCommand::SaveGraph,
            CycleV2AutomationCommand::Undo, CycleV2AutomationCommand::Redo }) {
        lifecycle.add(automationCommandName(operation));
    }
    fields.setProperty("lifecycleCommands", lifecycle);
    objectFor(capabilities)->setProperty("authoring", authoring);
    var impulseImport = makeObject();
    objectFor(impulseImport)->setProperty("command",
            automationCommandName(CycleV2AutomationCommand::ImportImpulseResponse));
    objectFor(impulseImport)->setProperty("requiredFields", var(StringArray { "nodeId", "path", "mode" }));
    objectFor(impulseImport)->setProperty("modes", var(StringArray { "direct", "modelled" }));
    objectFor(impulseImport)->setProperty("sourceNormalization", "Peak-normalized, first channel retained");
    objectFor(impulseImport)->setProperty("sampleRatePolicy", "Source samples retained without output-rate resampling");
    objectFor(capabilities)->setProperty("impulseResponseImport", impulseImport);
    var explanations = makeObject();
    objectFor(explanations)->setProperty("scopes", var(StringArray { "preset", "node", "group" }));
    objectFor(explanations)->setProperty("readCommand", automationCommandName(CycleV2AutomationCommand::InspectExplanation));
    objectFor(explanations)->setProperty("writeCommand", automationCommandName(CycleV2AutomationCommand::SetExplanation));
    objectFor(explanations)->setProperty("editorCommand", automationCommandName(CycleV2AutomationCommand::OpenExplanationEditor));
    objectFor(explanations)->setProperty("maximumTextLength", maximumExplanationLength);
    objectFor(explanations)->setProperty("groupCardCommand", automationCommandName(CycleV2AutomationCommand::SetGroupInformationCard));
    objectFor(explanations)->setProperty("maximumCardOffset", maximumInformationCardOffset);
    objectFor(capabilities)->setProperty("explanations", explanations);
    const File path = resolveCommandPath(stringProperty(command, "path"));
    if (path == File()) {
        return okResult("inspectSynthesisCapabilities", capabilities);
    }
    path.getParentDirectory().createDirectory();
    if (!path.replaceWithText(JSON::toString(capabilities))) {
        return failedResult("inspectSynthesisCapabilities", "Could not write capabilities: " + path.getFullPathName());
    }
    var data = makeObject();
    objectFor(data)->setProperty("path", path.getFullPathName());
    return okResult("inspectSynthesisCapabilities", data);
}

var CycleV2Automation::convertSynthesisParameter(const var& command) const {
    ParameterMappingContext context;
    const var inputValue = command.getProperty("value", var());
    const String direction = stringProperty(command, "direction", "toPhysical");
    if (!readContext(command, context) || !numeric(inputValue)
            || (direction != "toPhysical" && direction != "toStored")) {
        return failedResult("convertSynthesisParameter", "Requires a numeric value, valid context and direction toPhysical or toStored");
    }
    String error;
    const var result = SynthesisCapabilities::convertParameter(
            stringProperty(command, "typeId"), stringProperty(command, "parameterId"),
            (double) inputValue, direction == "toStored", context, error);
    return error.isNotEmpty() ? failedResult("convertSynthesisParameter", error)
            : okResult("convertSynthesisParameter", result);
}

}
