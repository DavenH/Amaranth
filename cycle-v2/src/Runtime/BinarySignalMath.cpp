#include "Runtime/BinarySignalMath.h"

#include "Graph/NodeGraph.h"

namespace CycleV2 {

bool BinarySignalMath::applyTransfer(
        Buffer<float> values,
        const SpectralMagnitudeTransfer* transfer,
        size_t channel,
        int harmonicCount) {
    if (transfer == nullptr || !transfer->isActive()) {
        return false;
    }
    applySpectralMagnitudeTransfer(values, *transfer, channel, harmonicCount);
    return true;
}

void BinarySignalMath::combine(
        Buffer<float> output,
        Buffer<float> right,
        BinarySignalOperation operation) {
    if (operation == BinarySignalOperation::Add) {
        output.add(right);
    } else {
        output.mul(right);
    }
}

void BinarySignalMath::clampOutputDomain(Buffer<float> output, PortDomain domain) {
    if (domain == PortDomain::SpectralMagnitudeSignal) {
        output.threshLT(0.f);
    }
}

}
