#ifndef CONTROLSCLIENT_H_
#define CONTROLSCLIENT_H_

#include "PanelControls.h"

class ControlsClient
{
public:
	PanelControls* getPanelControls() { return panelControls; }
	Component* getControlsComponent() { return panelControls; }

protected:
	ScopedPointer<PanelControls> panelControls;
};


#endif
