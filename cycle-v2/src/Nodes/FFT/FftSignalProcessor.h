#pragma once

#include "FftBlockwiseDsp.h"
#include "FftGridwiseDsp.h"
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
            SignalPayload& phase);
    void publishForwardGridResult(
            const std::vector<FftGridColumn>& columns,
            SignalPayload& output,
            bool magnitude) const;
    void publishInverseTraversalGrid(
            const SignalPayload& magnitude,
            const SignalPayload* phase,
            SignalPayload& output);

    FftBlockwiseDsp blockwiseDsp;
    FftGridwiseDsp gridwiseDsp;
};

}
