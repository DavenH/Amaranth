#pragma once

#include "TrimeshPanel3D.h"

#include <App/SingletonRepo.h>
#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>
#include <Inter/Interactor3D.h>
#include <Inter/MorphPositioner.h>

#include <cstdint>

namespace CycleV2 {

class TrimeshPanelBridge {
public:
    TrimeshPanelBridge();
    ~TrimeshPanelBridge();

    void syncFromNode(
            const Node& node,
            int rows,
            int columns);

    TrimeshPanel3D& getPanel3D() { return panel3D; }
    TrimeshPanelDataSource& getDataSource() { return dataSource; }
    Interactor3D& getInteractor3D() { return interactor3D; }
    TrimeshNodeModel& getModel() { return model; }
    Component* getPanelComponent();
    Component* getPanelComponentIfCreated();

private:
    class NodeMorphPositioner : public MorphPositioner {
    public:
        void setPosition(const MorphPosition& position, int primaryAxis);

        MorphPosition getMorphPosition() override { return morph; }
        MorphPosition getOffsetPosition(bool) override { return morph; }
        int getPrimaryDimension() override { return primaryDimension; }
        float getYellow() override { return morph.time.getCurrentValue(); }
        float getRed() override { return morph.red.getCurrentValue(); }
        float getBlue() override { return morph.blue.getCurrentValue(); }
        float getValue(int dim) override;

    private:
        MorphPosition morph { 0.5f, 0.5f, 0.5f };
        int primaryDimension {};
    };

    void updateRasterizer();

    SingletonRepo repo;
    NodeMorphPositioner morphPositioner;
    TrimeshNodeModel model;
    TrimeshPanelDataSource dataSource;
    Rasterization::TrilinearMeshRasterizer rasterizer;
    Interactor3D interactor3D;
    TrimeshPanel3D panel3D;
    uint64_t lastSyncedRevision { UINT64_MAX };
    int lastRows {};
    int lastColumns {};
    bool panelInitialised {};
};

}
