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
    void setGuideCurveProvider(GuideCurveProvider* provider);
    void prepare(
            Mesh& mesh,
            const MorphPosition& center,
            int primaryViewAxis,
            size_t maximumColumnCount,
            size_t maximumRowCount,
            PortDomain domain);

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
            Buffer<float> destination,
            PortDomain domain);
    bool renderMorphColumnsInto(
            Mesh& mesh,
            const MorphPosition* morphs,
            int primaryViewAxis,
            size_t columnCount,
            Buffer<float> destination,
            PortDomain domain);
    static MorphPosition morphForColumn(
            const MorphPosition& center,
            int primaryViewAxis,
            size_t index,
            size_t columnCount);
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

    TrimeshBlockwiseDsp blockwiseDsp;
    RenderCounters renderCounters;
    std::vector<float> preparationScratch;
};

}
