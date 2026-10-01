#pragma once

#include <UI/Panels/Panel3D.h>

#include <cstdint>
#include <vector>

#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"
#include "Nodes/Trimesh/Rendering/TrimeshGridRenderService.h"

namespace CycleV2 {

class TrimeshPanelDataSource : public Panel3D::DataRetriever {
public:
    struct RenderCounters {
        uint64_t sliceRebuilds {};
        uint64_t surfaceRebuilds {};
    };

    void rebuild(
            TrimeshNodeModel& model,
            int rows,
            int columns,
            PortDomain domain = PortDomain::TimeSignal,
            int midiNote = 48,
            int keyScaleAxis = -1);
    void rebuild(
            TrimeshNodeModel& model,
            int rows,
            int columns,
            const TrimeshRenderProfile& renderProfile,
            int midiNote = 48,
            int keyScaleAxis = -1);
    void rebuildSlice(
            TrimeshNodeModel& model,
            int rows,
            const TrimeshRenderProfile& renderProfile,
            int midiNote = 48);

    Buffer<float> getColumnArray() override;
    Buffer<float> getScalarSurfaceArray() override;
    const std::vector<Column>& getColumns() override;
    CriticalSection& getGridLock() override;
    uint64_t getScalarSurfaceRevision() const override { return scalarSurfaceRevision; }
    bool hasStableScalarSurfaceRevision() const override { return true; }

    const TrimeshRenderData& getRenderData() const { return renderData; }
    const std::vector<Column>& getPanelColumns() const { return panelColumns; }
    const RenderCounters& getRenderCounters() const { return renderCounters; }

private:
    TrimeshRenderData renderData;
    std::vector<float> storage;
    std::vector<float> scalarSurfaceStorage;
    std::vector<Column> panelColumns;
    CriticalSection gridLock;
    RenderCounters renderCounters;
    uint64_t scalarSurfaceRevision {};
};

}
