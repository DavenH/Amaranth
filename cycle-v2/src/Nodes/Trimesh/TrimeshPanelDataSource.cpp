#include "TrimeshPanelDataSource.h"

namespace CycleV2 {

void TrimeshPanelDataSource::rebuild(
        TrimeshNodeModel& model,
        int rows,
        int columns) {
    const ScopedLock lock(gridLock);

    renderData = model.renderGrid(rows, columns);
    storage = renderData.surface;
    panelColumns.clear();
    panelColumns.reserve((size_t) renderData.columns);

    if (renderData.rows <= 0 || renderData.columns <= 0 || storage.empty()) {
        return;
    }

    for (int column = 0; column < renderData.columns; ++column) {
        const size_t offset = (size_t) column * (size_t) renderData.rows;
        const float x = renderData.columns == 1
                ? 0.f
                : (float) column / (float) (renderData.columns - 1);

        panelColumns.emplace_back(
                storage.data() + offset,
                renderData.rows,
                x,
                0);
    }
}

Buffer<float> TrimeshPanelDataSource::getColumnArray() {
    const ScopedLock lock(gridLock);

    if (storage.empty()) {
        return {};
    }

    return { storage.data(), (int) storage.size() };
}

const std::vector<Column>& TrimeshPanelDataSource::getColumns() {
    return panelColumns;
}

CriticalSection& TrimeshPanelDataSource::getGridLock() {
    return gridLock;
}

}
