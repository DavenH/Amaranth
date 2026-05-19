#include "FftGridwiseDsp.h"

namespace CycleV2 {

std::vector<FftGridColumn> FftGridwiseDsp::forwardColumns(
        const std::vector<AudioProcessBlock>& timeColumns) {
    std::vector<FftGridColumn> columns;
    columns.reserve(timeColumns.size());

    for (const auto& timeColumn : timeColumns) {
        FftGridColumn column;
        column.magnitude.domain = PortDomain::SpectralMagnitudeSignal;
        column.magnitude.channelLayout = timeColumn.channelLayout;
        column.phase.domain = PortDomain::SpectralPhaseSignal;
        column.phase.channelLayout = timeColumn.channelLayout;
        blockwiseDsp.forward(timeColumn, column.magnitude, column.phase);
        columns.push_back(std::move(column));
    }

    return columns;
}

}
