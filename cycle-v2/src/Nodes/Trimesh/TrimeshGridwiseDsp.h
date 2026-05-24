#pragma once

#include "TrimeshBlockwiseDsp.h"

class Mesh;

namespace CycleV2 {

struct TrimeshGridColumn {
    AudioProcessBlock signal;
    MorphPosition morph;
};

class TrimeshGridwiseDsp {
public:
    std::vector<TrimeshGridColumn> renderColumns(
            Mesh& mesh,
            const MorphPosition& center,
            int primaryViewAxis,
            size_t columnCount,
            size_t frameCount,
            PortDomain domain,
            ChannelLayout channelLayout);

private:
    static MorphPosition morphForColumn(
            const MorphPosition& center,
            int primaryViewAxis,
            size_t index,
            size_t columnCount);

    TrimeshBlockwiseDsp blockwiseDsp;
};

}
