#pragma once

#include <Array/Buffer.h>
#include <Audio/CycleDsp/SpectralLayerCore.h>

#include <vector>

namespace CycleV2 {

struct GraphExecutionPlan;

enum class SpectralMagnitudeTransferMode {
    None,
    AddUnipolar,
    AddBipolar,
    MultiplyUnipolar,
    MultiplyBipolar
};

struct CompiledSpectralMagnitudeTransfer {
    SpectralMagnitudeTransferMode mode { SpectralMagnitudeTransferMode::None };
    int sourceBufferIndex { -1 };
    int sourceStepIndex { -1 };
    int sourceOutputIndex { -1 };
    std::vector<int> panStepIndices;

    bool isActive() const {
        return mode != SpectralMagnitudeTransferMode::None
                && sourceBufferIndex >= 0
                && sourceStepIndex >= 0;
    }
};

struct SpectralMagnitudeTransfer {
    SpectralMagnitudeTransferMode mode { SpectralMagnitudeTransferMode::None };
    float range { 0.5f };
    float leftPan { 1.f };
    float rightPan { 1.f };
    bool enabled { true };

    bool isActive() const { return mode != SpectralMagnitudeTransferMode::None; }
    bool isAdditive() const {
        return mode == SpectralMagnitudeTransferMode::AddUnipolar
                || mode == SpectralMagnitudeTransferMode::AddBipolar;
    }
    bool isBipolar() const {
        return mode == SpectralMagnitudeTransferMode::AddBipolar
                || mode == SpectralMagnitudeTransferMode::MultiplyBipolar;
    }
};

inline void applySpectralMagnitudeTransfer(
        Buffer<float> values,
        const SpectralMagnitudeTransfer& transfer,
        size_t channel,
        int harmonicCount) {
    if (!transfer.isActive()) {
        return;
    }
    if (!transfer.enabled) {
        values.set(transfer.isAdditive() ? 0.f : 1.f);
        return;
    }

    CycleDsp::SpectralLayerCore::shapeMagnitudeOperand(
            values,
            transfer.range,
            transfer.isAdditive(),
            transfer.isBipolar(),
            harmonicCount);
    const float pan = channel == 0 ? transfer.leftPan : transfer.rightPan;
    if (transfer.isAdditive()) {
        values.mul(pan);
    } else {
        CycleDsp::SpectralLayerCore::applyMultiplicativePan(values, pan);
    }
}

SpectralMagnitudeTransfer resolveSpectralMagnitudeTransfer(
        const GraphExecutionPlan& plan,
        const CompiledSpectralMagnitudeTransfer& compiled);

}
