#pragma once

#include "PanelControls.h"

class ControlsClient
{
public:
	PanelControls* getPanelControls() { return panelControls.get(); }
	Component* getControlsComponent() { return panelControls.get(); }

protected:
	std::unique_ptr<PanelControls> panelControls;
};
