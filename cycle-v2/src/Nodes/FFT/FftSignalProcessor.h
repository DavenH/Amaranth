#pragma once

#include "FftBlockwiseDsp.h"
#include "../../Runtime/AudioProcessContextUtils.h"

#include <array>
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
    FftBlockwiseDsp& blockwiseFor(size_t frameCount, size_t channel);
    void publishForwardTraversalGrids(
            const SignalPayload& input,
            SignalPayload& magnitude,
            SignalPayload& phase,
            size_t inputChannel,
            size_t outputChannel,
            const AudioProcessWorkArena* arena);
    TraversalGridMetadata frequencyMetadataFor(
            const SignalTraversalGrid& inputGrid,
            const SignalPayload& output,
            size_t rows) const;
    void publishInverseTraversalGrid(
            const SignalPayload& magnitude,
            const SignalPayload* phase,
            SignalPayload& output,
            size_t channel,
            bool useHalfCycleCarry,
            const AudioProcessWorkArena* arena);

    std::array<FftBlockwiseDsp, 2> blockwiseDsp;
    std::array<std::vector<std::unique_ptr<FftBlockwiseDsp>>, 2>
            preparedBlockwiseDsp;
    FftBlockwiseDsp traversalDsp;
    AudioProcessBlock scratchTimeColumn;
    AudioProcessBlock scratchMagnitudeColumn;
    AudioProcessBlock scratchPhaseColumn;
    AudioProcessBlock scratchOutputColumn;
};

}
