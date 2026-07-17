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
    template<typename RenderColumn>
    void renderColumnRange(
            Mesh& mesh,
            const MorphPosition& center,
            int primaryViewAxis,
            size_t columnCount,
            RenderColumn renderColumn) {
        blockwiseDsp.setMesh(&mesh);
        blockwiseDsp.setPrimaryViewAxis(primaryViewAxis);

        for (size_t index = 0; index < columnCount; ++index) {
            const MorphPosition morph = morphForColumn(
                    center,
                    primaryViewAxis,
                    index,
                    columnCount);
            blockwiseDsp.setMorphPosition(morph);
            renderColumn(index, morph);
        }
    }

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
