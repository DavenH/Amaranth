#pragma once

#include "TrimeshBlockwiseDsp.h"

class Mesh;

namespace CycleV2 {

struct TrimeshGridColumn {
    SignalPayload signal;
    MorphPosition morph;
};

class TrimeshGridwiseDsp {
public:
    struct RenderCounters {
        size_t sliceCount {};
        size_t bakeCount {};
    };

    void setCyclic(bool shouldWrap);
    void prepare(
            Mesh& mesh,
            const MorphPosition& center,
            int primaryViewAxis,
            size_t maximumColumnCount,
            size_t maximumRowCount);

    std::vector<TrimeshGridColumn> renderColumns(
            Mesh& mesh,
            const MorphPosition& center,
            int primaryViewAxis,
            size_t columnCount,
            size_t frameCount,
            PortDomain domain,
            ChannelLayout channelLayout);
    bool renderColumnsInto(
            Mesh& mesh,
            const MorphPosition& center,
            int primaryViewAxis,
            size_t columnCount,
            Buffer<float> destination);
    const RenderCounters& counters() const { return renderCounters; }
    void resetCounters() { renderCounters = {}; }

private:
    static MorphPosition morphForColumn(
            const MorphPosition& center,
            int primaryViewAxis,
            size_t index,
            size_t columnCount);

    TrimeshBlockwiseDsp blockwiseDsp;
    RenderCounters renderCounters;
    std::vector<float> preparationScratch;
};

}
