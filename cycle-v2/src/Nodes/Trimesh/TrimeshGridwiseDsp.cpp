#include "TrimeshGridwiseDsp.h"

#include <Curve/Mesh/Vertex.h>

namespace CycleV2 {

void TrimeshGridwiseDsp::setCyclic(bool shouldWrap) {
    blockwiseDsp.setCyclic(shouldWrap);
}

void TrimeshGridwiseDsp::prepare(
        Mesh& mesh,
        const MorphPosition& center,
        int primaryViewAxis,
        size_t maximumColumnCount,
        size_t maximumRowCount) {
    preparationScratch.resize(maximumRowCount);
    blockwiseDsp.setMesh(&mesh);
    blockwiseDsp.setPrimaryViewAxis(primaryViewAxis);
    for (size_t i = 0; i < maximumColumnCount; ++i) {
        blockwiseDsp.setMorphPosition(morphForColumn(
                center, primaryViewAxis, i, maximumColumnCount));
        blockwiseDsp.renderCycleInto(Buffer<float>(
                preparationScratch.data(), (int) preparationScratch.size()));
    }
    resetCounters();
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

bool TrimeshGridwiseDsp::renderColumnsInto(
        Mesh& mesh,
        const MorphPosition& center,
        int primaryViewAxis,
        size_t columnCount,
        Buffer<float> destination) {
    if (columnCount == 0 || destination.empty()
            || destination.size() % (int) columnCount != 0) {
        return false;
    }

    const int rowCount = destination.size() / (int) columnCount;
    blockwiseDsp.setMesh(&mesh);
    blockwiseDsp.setPrimaryViewAxis(primaryViewAxis);
    for (size_t i = 0; i < columnCount; ++i) {
        blockwiseDsp.setMorphPosition(morphForColumn(
                center, primaryViewAxis, i, columnCount));
        blockwiseDsp.renderCycleInto(destination.section((int) i * rowCount, rowCount));
        ++renderCounters.sliceCount;
        ++renderCounters.bakeCount;
    }

    return true;
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
