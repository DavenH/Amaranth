#include <Array/Buffer.h>

#include "Nodes/Trimesh/Rendering/TrimeshGridRenderService.h"

#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Nodes/Trimesh/Dsp/TrimeshGridwiseDsp.h"
#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"

namespace CycleV2 {

TrimeshRenderData TrimeshGridRenderService::renderGrid(
        TrimeshNodeModel& model,
        int rows,
        int columns,
        PortDomain domain,
        int midiNote) {
    return renderGrid(
            model,
            rows,
            columns,
            TrimeshRenderProfile::fromDomain(domain),
            midiNote);
}

TrimeshRenderData TrimeshGridRenderService::renderGrid(
        TrimeshNodeModel& model,
        int rows,
        int columns,
        const TrimeshRenderProfile& renderProfile,
        int midiNote) {
    rows = jmax(2, rows);
    columns = jmax(2, columns);
    const PortDomain domain = renderProfile.getDomain();
    const bool cyclic = domain == PortDomain::TimeSignal;

    TrimeshRenderData result;
    result.domain = domain;
    result.rows = rows;
    result.columns = columns;
    result.cyclic = cyclic;

    TrimeshBlockwiseDsp blockwiseDsp;
    SignalPayload slice;
    blockwiseDsp.setGuideCurveProvider(model.guideCurveProvider.get());
    blockwiseDsp.setMesh(&model.mesh());
    blockwiseDsp.setMorphPosition(model.morph);
    blockwiseDsp.setPrimaryViewAxis(model.primaryViewAxis);
    blockwiseDsp.setCyclic(cyclic);
    blockwiseDsp.setFrequencyMidiNote(midiNote);
    blockwiseDsp.renderCycle((size_t) rows, domain, ChannelLayout::LinkedStereo, slice);
    if (domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal) {
        const std::vector<float> rawSlice(
                slice.block.samples.begin(),
                slice.block.samples.end());
        result.slice = renderProfile.mapGridToDisplay(
                rawSlice,
                1,
                (size_t) rows,
                midiNote);
    } else {
        renderProfile.mapValuesToDisplay(Buffer<float>(
                slice.block.samples.data(),
                (int) slice.block.samples.size()));
        result.slice.assign(slice.block.samples.begin(), slice.block.samples.end());
    }

    TrimeshGridwiseDsp gridwiseDsp;
    gridwiseDsp.setCyclic(cyclic);
    gridwiseDsp.setGuideCurveProvider(model.guideCurveProvider.get());
    gridwiseDsp.setFrequencyMidiNote(midiNote);
    const auto gridColumns = gridwiseDsp.renderColumns(
            model.mesh(),
            model.morph,
            model.primaryViewAxis,
            (size_t) columns,
            (size_t) rows,
            domain,
            ChannelLayout::LinkedStereo);

    result.surface.reserve((size_t) rows * (size_t) columns);

    for (auto column : gridColumns) {
        result.surface.insert(
                result.surface.end(),
                column.signal.block.samples.begin(),
                column.signal.block.samples.end());
    }
    result.linearFrequencySurface = renderProfile.mapTrimeshValuesToDisplay(
            result.surface);
    result.surface = renderProfile.mapGridToDisplay(
            result.surface,
            (size_t) columns,
            (size_t) rows,
            midiNote);

    return result;
}

}
