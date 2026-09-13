#include "Graph/TrimeshSignalSemantics.h"

#include "Graph/NodeParameterMap.h"

namespace CycleV2 {

PortDomain TrimeshSignalSemantics::domain(const Node& node) {
    const String signalType = parameterValueForNode(node, "signalType", "time");
    if (signalType == "spectralMagnitude") {
        return PortDomain::SpectralMagnitudeSignal;
    }
    if (signalType == "spectralPhase") {
        return PortDomain::SpectralPhaseSignal;
    }
    return PortDomain::TimeSignal;
}

bool TrimeshSignalSemantics::isBipolar(const Node& node) {
    return isBipolar(node.parameters);
}

bool TrimeshSignalSemantics::isBipolar(
        const std::vector<NodeParameter>& parameters) {
    const NodeParameterMap parameterMap(parameters);
    const String signalType = parameterMap.stringValue("signalType", "time");
    if (signalType != "spectralMagnitude") {
        return true;
    }
    return parameterMap.stringValue("polarity", "unipolar") == "bipolar";
}

}
