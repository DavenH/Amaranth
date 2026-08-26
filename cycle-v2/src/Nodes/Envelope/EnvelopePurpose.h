#pragma once

#include "Graph/NodeGraph.h"

#include <array>

namespace CycleV2 {

enum class EnvelopePurpose {
    Control,
    Volume,
    Pitch,
    Scratch
};

inline constexpr std::array<EnvelopePurpose, 4> kEnvelopePurposes {
    EnvelopePurpose::Control,
    EnvelopePurpose::Volume,
    EnvelopePurpose::Pitch,
    EnvelopePurpose::Scratch
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
