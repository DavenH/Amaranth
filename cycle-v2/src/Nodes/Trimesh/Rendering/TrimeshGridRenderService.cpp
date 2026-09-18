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
        int midiNote,
        int keyScaleAxis) {
    rows = jmax(2, rows);
    columns = jmax(2, columns);
    const PortDomain domain = renderProfile.getDomain();
    const bool cyclic = domain == PortDomain::TimeSignal;
    const bool pitchSpansColumns = (domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal)
            && model.primaryViewAxis == keyScaleAxis;

    TrimeshRenderData result;
    result.domain = domain;
    result.rows = rows;
    result.columns = columns;
    result.midiNote = midiNote;
    result.cyclic = cyclic;
    result.pitchSpansColumns = pitchSpansColumns;

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
    result.surface.resize((size_t) rows * (size_t) columns);
    Buffer<float> surface(result.surface.data(), (int) result.surface.size());
    if (pitchSpansColumns) {
        gridwiseDsp.renderPitchColumnsInto(
                model.mesh(),
                model.morph,
                model.primaryViewAxis,
                (size_t) columns,
                surface,
                domain);
    } else {
        gridwiseDsp.renderColumnsInto(
                model.mesh(),
                model.morph,
                model.primaryViewAxis,
                (size_t) columns,
                surface,
                domain);
    }
    result.linearFrequencySurface = renderProfile.mapTrimeshValuesToDisplay(
            result.surface);
    result.surface = pitchSpansColumns
            ? result.linearFrequencySurface
            : renderProfile.mapGridToDisplay(
                    result.surface,
                    (size_t) columns,
                    (size_t) rows,
                    midiNote);

    return result;
}

}
