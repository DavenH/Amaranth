#pragma once

#include "Nodes/Trimesh/TrimeshNodeModel.h"

#include <UI/Panels/Panel3D.h>

#include <vector>

namespace CycleV2 {

class TrimeshPanelDataSource : public Panel3D::DataRetriever {
public:
    void rebuild(
            TrimeshNodeModel& model,
            int rows,
            int columns,
            PortDomain domain = PortDomain::TimeSignal,
            int midiNote = 48);
    void rebuild(
            TrimeshNodeModel& model,
            int rows,
            int columns,
            const TrimeshRenderProfile& renderProfile,
            int midiNote = 48);

    Buffer<float> getColumnArray() override;
    const std::vector<Column>& getColumns() override;
    CriticalSection& getGridLock() override;

    const TrimeshRenderData& getRenderData() const { return renderData; }

private:
    TrimeshRenderData renderData;
    std::vector<float> storage;
    std::vector<Column> panelColumns;
    CriticalSection gridLock;
};

}
