#pragma once

#include "../../Graph/NodeGraph.h"

namespace CycleV2 {

enum class EnvelopePurpose {
    Control,
    Volume,
    Pitch,
    Scratch
};

EnvelopePurpose envelopePurposeFromString(const String& value);
String envelopePurposeToString(EnvelopePurpose purpose);
String envelopePurposeLabel(EnvelopePurpose purpose);
PortDomain envelopeOutputDomain(EnvelopePurpose purpose);
ConnectionKind envelopeConnectionKind(EnvelopePurpose purpose);
bool envelopePurposeAllowsLogarithmic(EnvelopePurpose purpose);
EnvelopePurpose envelopePurposeFor(const Node& node);
void applyEnvelopePurpose(Node& node);

}
