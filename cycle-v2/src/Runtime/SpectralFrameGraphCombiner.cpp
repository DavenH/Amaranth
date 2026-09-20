#include "Runtime/SpectralFrameGraphCombiner.h"

#include "Graph/NodeGraph.h"

#include <Audio/CycleDsp/SpectralLayerCore.h>
#include <Util/Arithmetic.h>

namespace CycleV2 {

SpectralFrameGraphCombiner::SpectralFrameGraphCombiner(
        const CycleDsp::SpectralFrameCapture& captureToUse,
        int activeHarmonicCountToUse) :
        capture             (captureToUse)
    ,   activeHarmonicCount (activeHarmonicCountToUse) {
}

void SpectralFrameGraphCombiner::applyPan(
        PortDomain domain,
        Buffer<float> source,
        Buffer<float> secondarySource,
        Buffer<float> left,
        Buffer<float> right,
        float pan,
        bool multiplicative) {
    float leftPan {};
    float rightPan {};
    Arithmetic::getPans(pan, leftPan, rightPan);
    source.copyTo(left);
    secondarySource.copyTo(right);
    if (domain == PortDomain::SpectralMagnitudeSignal && multiplicative) {
        CycleDsp::SpectralLayerCore::applyMultiplicativePan(left, leftPan);
        CycleDsp::SpectralLayerCore::applyMultiplicativePan(right, rightPan);
    } else {
        left.mul(leftPan);
        right.mul(rightPan);
    }
}

void SpectralFrameGraphCombiner::add(
        SpectralFrameOperand left,
        SpectralFrameOperand right,
        Buffer<float> output,
        Buffer<float> scratch,
        PortDomain outputDomain,
        int channel) const {
    output.zero();
    if (left.connected) {
        copyOperand(output, left, channel);
    }
    if (right.connected) {
        if (right.transfer != nullptr && right.transfer->isActive()) {
            copyOperand(scratch, right, channel);
            BinarySignalMath::combine(output, scratch, BinarySignalOperation::Add);
        } else {
            BinarySignalMath::combine(output, right.values, BinarySignalOperation::Add);
        }
    }
    BinarySignalMath::clampOutputDomain(output, outputDomain);
}

void SpectralFrameGraphCombiner::multiply(
        SpectralFrameOperand left,
        SpectralFrameOperand right,
        Buffer<float> output,
        Buffer<float> scratch,
        PortDomain outputDomain,
        int channel) const {
    copyOperand(output, left, channel);
    if (right.transfer != nullptr && right.transfer->isActive()) {
        copyOperand(scratch, right, channel);
        BinarySignalMath::combine(output, scratch, BinarySignalOperation::Multiply);
    } else {
        BinarySignalMath::combine(output, right.values, BinarySignalOperation::Multiply);
    }
    BinarySignalMath::clampOutputDomain(output, outputDomain);
}

void SpectralFrameGraphCombiner::copyOperand(
        Buffer<float> output,
        SpectralFrameOperand operand,
        int channel) const {
    operand.values.copyTo(output);
    if (BinarySignalMath::applyTransfer(
            output,
            operand.transfer,
            (size_t) channel,
            activeHarmonicCount)) {
        capture.capture(
                CycleDsp::SpectralStage::MagnitudeOperand,
                channel,
                output.section(1, activeHarmonicCount));
    }
}

}
