#include <cmath>
#include <limits>

#include "App/CycleV2Automation.h"

#include "App/CycleV2AutomationProtocol.h"
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
    const var capabilities = SynthesisCapabilities::exportJSON(context);
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
