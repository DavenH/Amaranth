#include "Nodes/Trimesh/Dsp/TrimeshGridwiseDsp.h"

#include <Curve/Mesh/Vertex.h>

namespace CycleV2 {

int TrimeshGridwiseDsp::noiseSeedOffsetForColumn(
        size_t columnIndex,
        PortDomain domain) {
    const bool spectral = domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal;
    const int columnStride = spectral ? 1997 : 6197;
    const size_t wrappedColumn = columnIndex % GuideCurveProvider::tableSize;
    return (int) ((wrappedColumn * (size_t) columnStride)
            % GuideCurveProvider::tableSize);
}

void TrimeshGridwiseDsp::setCyclic(bool shouldWrap) {
    blockwiseDsp.setCyclic(shouldWrap);
}

void TrimeshGridwiseDsp::setGuideCurveProvider(GuideCurveProvider* provider) {
    blockwiseDsp.setGuideCurveProvider(provider);
}

void TrimeshGridwiseDsp::setVoiceLifecycleSeed(uint32_t seed) {
    blockwiseDsp.setVoiceLifecycleSeed(seed);
}

void TrimeshGridwiseDsp::setFrequencyMidiNote(int midiNote) {
    blockwiseDsp.setFrequencyMidiNote(midiNote);
}

void TrimeshGridwiseDsp::prepareSampling(size_t maximumRowCount) {
    blockwiseDsp.prepareSampling(maximumRowCount);
}

void TrimeshGridwiseDsp::prepare(
        Mesh& mesh,
        const MorphPosition& center,
        int primaryViewAxis,
        size_t maximumColumnCount,
        size_t maximumRowCount,
        PortDomain domain) {
    preparationScratch.resize(maximumRowCount);
    prepareSampling(maximumRowCount);
    renderColumnRange(
            mesh,
            center,
            primaryViewAxis,
            maximumColumnCount,
            [this, domain](size_t index, const MorphPosition&) {
                blockwiseDsp.renderCycleWithNoiseSeedOffsetInto(
                        Buffer<float>(
                                preparationScratch.data(),
                                (int) preparationScratch.size()),
                        domain,
                        noiseSeedOffsetForColumn(index, domain));
            });
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

    renderColumnRange(
            mesh,
            center,
            primaryViewAxis,
            columnCount,
            [this, &columns, frameCount, domain, channelLayout](
                    size_t index,
                    const MorphPosition& morph) {
                TrimeshGridColumn column;
                column.morph = morph;
                blockwiseDsp.renderCycleWithNoiseSeedOffset(
                        frameCount,
                        domain,
                        channelLayout,
                        column.signal,
                        noiseSeedOffsetForColumn(index, domain));
                columns.push_back(std::move(column));
            });

    return columns;
}

bool TrimeshGridwiseDsp::renderColumnsInto(
        Mesh& mesh,
        const MorphPosition& center,
        int primaryViewAxis,
        size_t columnCount,
        Buffer<float> destination,
        PortDomain domain) {
    if (columnCount == 0 || destination.empty()
            || destination.size() % (int) columnCount != 0) {
        return false;
    }

    const int rowCount = destination.size() / (int) columnCount;
    renderColumnRange(
            mesh,
            center,
            primaryViewAxis,
            columnCount,
            [this, destination, rowCount, domain](
                    size_t index,
                    const MorphPosition&) {
                blockwiseDsp.renderCycleWithNoiseSeedOffsetInto(
                        destination.section((int) index * rowCount, rowCount),
                        domain,
                        noiseSeedOffsetForColumn(index, domain));
                ++renderCounters.sliceCount;
                ++renderCounters.bakeCount;
            });

    return true;
}

bool TrimeshGridwiseDsp::renderMorphColumnsInto(
        Mesh& mesh,
        const MorphPosition* morphs,
        int primaryViewAxis,
        size_t columnCount,
        Buffer<float> destination,
        PortDomain domain) {
    if (morphs == nullptr || columnCount == 0 || destination.empty()
            || destination.size() % (int) columnCount != 0) {
        return false;
    }

    const int rowCount = destination.size() / (int) columnCount;
    blockwiseDsp.setMesh(&mesh);
    blockwiseDsp.setPrimaryViewAxis(primaryViewAxis);
    for (size_t index = 0; index < columnCount; ++index) {
        blockwiseDsp.setMorphPosition(morphs[index]);
        blockwiseDsp.renderCycleWithNoiseSeedOffsetInto(
                destination.section((int) index * rowCount, rowCount),
                domain,
                noiseSeedOffsetForColumn(index, domain));
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
