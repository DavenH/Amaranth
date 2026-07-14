#pragma once

#include "FftBlockwiseDsp.h"
#include "../../Runtime/AudioProcessContextUtils.h"

#include <memory>
#include <vector>

namespace CycleV2 {

class FftSignalProcessor {
public:
    void prepareExecution(size_t maximumFrameCount);
    void processForward(AudioProcessContext& context);
    void processInverse(AudioProcessContext& context);
    void processInverse(AudioProcessContext& context, bool useHalfCycleCarry);

private:
    FftBlockwiseDsp& blockwiseFor(size_t frameCount);
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
            bool useHalfCycleCarry,
            const AudioProcessWorkArena* arena);

    FftBlockwiseDsp blockwiseDsp;
    std::vector<std::unique_ptr<FftBlockwiseDsp>> preparedBlockwiseDsp;
    FftBlockwiseDsp traversalDsp;
    AudioProcessBlock scratchTimeColumn;
    AudioProcessBlock scratchMagnitudeColumn;
    AudioProcessBlock scratchPhaseColumn;
    AudioProcessBlock scratchOutputColumn;
};

}
