#pragma once

#include "PanelControls.h"

class ControlsClient
{
public:
    [[nodiscard]] PanelControls* getPanelControls() const { return panelControls.get(); }
    [[nodiscard]] Component* getControlsComponent() const { return panelControls.get(); }

protected:
    std::unique_ptr<PanelControls> panelControls;
};
