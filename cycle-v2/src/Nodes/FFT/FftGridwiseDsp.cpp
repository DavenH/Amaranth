#include "FftGridwiseDsp.h"

namespace CycleV2 {

std::vector<FftGridColumn> FftGridwiseDsp::forwardColumns(
        const std::vector<AudioProcessBlock>& timeColumns) {
    std::vector<FftGridColumn> columns;
    columns.reserve(timeColumns.size());

    for (const auto& timeColumn : timeColumns) {
        FftGridColumn column;
        column.magnitude.domain = PortDomain::SpectralMagnitudeSignal;
        column.magnitude.channelLayout = ChannelLayout::LinkedStereo;
        column.phase.domain = PortDomain::SpectralPhaseSignal;
        column.phase.channelLayout = ChannelLayout::LinkedStereo;
        blockwiseDsp.forward(timeColumn, column.magnitude.block, column.phase.block);
        columns.push_back(std::move(column));
    }

    return columns;
}

}
