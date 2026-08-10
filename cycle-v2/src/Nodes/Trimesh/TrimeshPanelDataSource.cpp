#include "TrimeshPanelDataSource.h"

#include "TrimeshRenderProfile.h"

namespace CycleV2 {

void TrimeshPanelDataSource::rebuild(
        TrimeshNodeModel& model,
        int rows,
        int columns,
        PortDomain domain,
        int midiNote) {
    rebuild(
            model,
            rows,
            columns,
            TrimeshRenderProfile::fromDomain(domain),
            midiNote);
}

void TrimeshPanelDataSource::rebuild(
        TrimeshNodeModel& model,
        int rows,
        int columns,
        const TrimeshRenderProfile& renderProfile,
        int midiNote) {
    const ScopedLock lock(gridLock);

    renderData = model.renderGrid(rows, columns, renderProfile, midiNote);
    storage = renderData.linearFrequencySurface.empty()
            ? renderData.surface
            : renderData.linearFrequencySurface;
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
                (char) midiNote);
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
