#pragma once

#include "FftBlockwiseDsp.h"

namespace CycleV2 {

struct FftGridColumn {
    AudioProcessBlock magnitude;
    AudioProcessBlock phase;
};

class FftGridwiseDsp {
public:
    std::vector<FftGridColumn> forwardColumns(const std::vector<AudioProcessBlock>& timeColumns);

private:
    FftBlockwiseDsp blockwiseDsp;
};

}
