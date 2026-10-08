#include "Nodes/Trimesh/Panel/TrimeshPanelDataSource.h"

#include <App/AppConstants.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegionMapping.h>

#include <algorithm>
#include <array>

#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"

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

    ++renderCounters.sliceRebuilds;
    ++renderCounters.surfaceRebuilds;
    renderData = TrimeshGridRenderService::renderGrid(
            model,
            rows,
            columns,
            renderProfile,
            midiNote,
            keyScaleAxis);
    storage = renderData.linearFrequencySurface.empty()
            ? renderData.surface
            : renderData.linearFrequencySurface;
    scalarSurfaceStorage = renderData.pitchSpansColumns
            ? renderProfile.mapPitchColumnsToDisplay(
                    renderData.surface,
                    (size_t) renderData.columns,
                    (size_t) renderData.rows)
            : renderData.surface;
    ++scalarSurfaceRevision;
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

void TrimeshPanelDataSource::rebuildSlice(
        TrimeshNodeModel& model,
        int rows,
        const TrimeshRenderProfile& renderProfile,
        int midiNote) {
    const ScopedLock lock(gridLock);

    ++renderCounters.sliceRebuilds;
    renderData.slice = TrimeshGridRenderService::renderSlice(
            model,
            rows,
            renderProfile,
            midiNote);
}

Buffer<float> TrimeshPanelDataSource::getColumnArray() {
    const ScopedLock lock(gridLock);

    if (storage.empty()) {
        return {};
    }

    return { storage.data(), (int) storage.size() };
}

Buffer<float> TrimeshPanelDataSource::getScalarSurfaceArray() {
    const ScopedLock lock(gridLock);

    if (scalarSurfaceStorage.empty()) {
        return {};
    }

    return { scalarSurfaceStorage.data(), (int) scalarSurfaceStorage.size() };
}

const std::vector<Column>& TrimeshPanelDataSource::getColumns() {
    return panelColumns;
}

bool TrimeshPanelDataSource::copyColumnAtMorph(
        float morphPosition,
        std::vector<float>& values,
        int& midiNote) {
    const ScopedLock lock(gridLock);
    if (panelColumns.empty()) {
        values.clear();
        return false;
    }

    const int index = jlimit(
            0,
            (int) panelColumns.size() - 1,
            roundToInt(morphPosition * (float) (panelColumns.size() - 1)));
    const Column& column = panelColumns[(size_t) index];
    midiNote = static_cast<unsigned char>(column.midiKey);
    values.resize((size_t) column.size());
    std::copy_n(column.get(), column.size(), values.data());
    return true;
}

CriticalSection& TrimeshPanelDataSource::getGridLock() {
    return gridLock;
}

}
