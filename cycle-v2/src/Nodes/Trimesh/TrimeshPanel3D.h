#pragma once

#include "TrimeshPanelDataSource.h"

#include <UI/Panels/Panel3D.h>

namespace CycleV2 {

class TrimeshPanel3D : public Panel3D {
public:
    TrimeshPanel3D(SingletonRepo* repo, TrimeshPanelDataSource& dataSource);

    bool shouldDrawGrid() override { return true; }
    void panelResized() override;
    void postVertsDraw() override {}

private:
    TrimeshPanelDataSource& dataSource;
};

}
