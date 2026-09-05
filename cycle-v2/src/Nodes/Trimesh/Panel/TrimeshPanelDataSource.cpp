#include "Nodes/Trimesh/Panel/TrimeshPanelDataSource.h"

#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"

#include <App/AppConstants.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegionMapping.h>

#include <array>

namespace CycleV2 {

namespace {

int spectralRegionSizeForMidiNote(int midiNote) {
    static const std::array<int, Constants::HighestMidiNote + 1> regionSizes = [] {
        std::array<int, Constants::HighestMidiNote + 1> result {};
        for (int note = Constants::LowestMidiNote;
                note <= Constants::HighestMidiNote;
                ++note) {
            result[(size_t) note] = LogRegionMapping(note).regionSize();
        }
        return result;
    }();
    return regionSizes[(size_t) jlimit(
            (int) Constants::LowestMidiNote,
            (int) Constants::HighestMidiNote,
            midiNote)];
}

}

void TrimeshPanelDataSource::rebuild(
        TrimeshNodeModel& model,
        int rows,
        int columns,
        PortDomain domain,
        int midiNote,
        int keyScaleAxis) {
    rebuild(
            model,
            rows,
            columns,
            TrimeshRenderProfile::fromDomain(domain),
            midiNote,
            keyScaleAxis);
}

void TrimeshPanelDataSource::rebuild(
        TrimeshNodeModel& model,
        int rows,
        int columns,
        const TrimeshRenderProfile& renderProfile,
        int midiNote,
        int keyScaleAxis) {
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

    const bool pitchSpansColumns = renderProfile.getSliceStyle().isSpectral()
            && model.getPrimaryViewAxis() == keyScaleAxis;
    const Range<int> midiRange {
            Constants::LowestMidiNote,
            Constants::HighestMidiNote
    };

    for (int column = 0; column < renderData.columns; ++column) {
        const size_t offset = (size_t) column * (size_t) renderData.rows;
        const float x = renderData.columns == 1
                ? 0.f
                : (float) column / (float) (renderData.columns - 1);
        const int columnMidiNote = pitchSpansColumns
                ? Arithmetic::getGraphicNoteForValue(x, midiRange)
                : midiNote;
        const int columnSize = pitchSpansColumns
                ? jmin(renderData.rows, spectralRegionSizeForMidiNote(columnMidiNote))
                : renderData.rows;

        panelColumns.emplace_back(
                storage.data() + offset,
                columnSize,
                x,
                (char) columnMidiNote);
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
