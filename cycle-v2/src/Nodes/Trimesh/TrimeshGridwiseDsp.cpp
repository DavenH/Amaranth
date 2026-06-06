#include "TrimeshGridwiseDsp.h"

#include <Curve/Mesh/Vertex.h>

namespace CycleV2 {

void TrimeshGridwiseDsp::setCyclic(bool shouldWrap) {
    blockwiseDsp.setCyclic(shouldWrap);
}

std::vector<TrimeshGridColumn> TrimeshGridwiseDsp::renderColumns(
        Mesh& mesh,
        const MorphPosition& center,
        int primaryViewAxis,
        size_t columnCount,
        size_t frameCount,
        PortDomain domain,
        ChannelLayout channelLayout) {
    std::vector<TrimeshGridColumn> columns;
    columns.reserve(columnCount);

    blockwiseDsp.setMesh(&mesh);
    blockwiseDsp.setPrimaryViewAxis(primaryViewAxis);

    for (size_t i = 0; i < columnCount; ++i) {
        TrimeshGridColumn column;
        column.morph = morphForColumn(center, primaryViewAxis, i, columnCount);
        blockwiseDsp.setMorphPosition(column.morph);
        blockwiseDsp.renderCycle(frameCount, domain, channelLayout, column.signal);
        columns.push_back(std::move(column));
    }

    return columns;
}

MorphPosition TrimeshGridwiseDsp::morphForColumn(
        const MorphPosition& center,
        int primaryViewAxis,
        size_t index,
        size_t columnCount) {
    MorphPosition morph = center;

    if (columnCount <= 1) {
        return morph;
    }

    const float position = (float) index / (float) (columnCount - 1);

    switch (primaryViewAxis) {
        case Vertex::Time:
            morph.time.setValueDirect(position);
            break;

        case Vertex::Red:
            morph.red.setValueDirect(position);
            break;

        case Vertex::Blue:
            morph.blue.setValueDirect(position);
            break;

        default:
            break;
    }

    return morph;
}

}
