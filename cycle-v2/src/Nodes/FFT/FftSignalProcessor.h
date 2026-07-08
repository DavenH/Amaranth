#pragma once

#include "FftBlockwiseDsp.h"
#include "../../Runtime/AudioProcessContextUtils.h"

namespace CycleV2 {

class FftSignalProcessor {
public:
    void processForward(AudioProcessContext& context);
    void processInverse(AudioProcessContext& context);

private:
    void publishForwardTraversalGrids(
            const SignalPayload& input,
            SignalPayload& magnitude,
            SignalPayload& phase,
            const AudioProcessWorkArena* arena);
    TraversalGridMetadata frequencyMetadataFor(
            const SignalTraversalGrid& inputGrid,
            const SignalPayload& output,
            size_t rows) const;
    void copyGridColumnToBlock(const SignalTraversalGrid& grid, size_t column, AudioProcessBlock& block) const;
    void copyBlockToGridColumn(const AudioProcessBlock& block, SignalTraversalGrid& grid, size_t column) const;
    void publishInverseTraversalGrid(
            const SignalPayload& magnitude,
            const SignalPayload* phase,
            SignalPayload& output,
            const AudioProcessWorkArena* arena);

    FftBlockwiseDsp blockwiseDsp;
    AudioProcessBlock scratchTimeColumn;
    AudioProcessBlock scratchMagnitudeColumn;
    AudioProcessBlock scratchPhaseColumn;
    AudioProcessBlock scratchOutputColumn;
};

}
