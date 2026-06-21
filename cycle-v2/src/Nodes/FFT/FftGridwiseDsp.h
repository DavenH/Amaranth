#pragma once

#include "FftBlockwiseDsp.h"

namespace CycleV2 {

struct FftGridColumn {
    SignalPayload magnitude;
    SignalPayload phase;
};

class FftGridwiseDsp {
public:
    std::vector<FftGridColumn> forwardColumns(const std::vector<AudioProcessBlock>& timeColumns);

private:
    FftBlockwiseDsp blockwiseDsp;
};

}
