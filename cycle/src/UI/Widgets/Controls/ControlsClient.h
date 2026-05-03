#pragma once

#include "PanelControls.h"

class ControlsClient
{
public:
    virtual ~ControlsClient() = default;

    [[nodiscard]] PanelControls* getPanelControls() const { return panelControls.get(); }
    [[nodiscard]] Component* getControlsComponent() const { return panelControls.get(); }

    virtual void reconcileLoadedState() {}

protected:
    std::unique_ptr<PanelControls> panelControls;
};
