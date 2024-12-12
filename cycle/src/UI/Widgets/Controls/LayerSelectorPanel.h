#pragma once
#include <Obj/Ref.h>
#include "SelectorPanel.h"

class SingletonRepo;
class LayerSelectionClient;

class LayerSelectorPanel :
	public SelectorPanel {
public:
	LayerSelectorPanel(SingletonRepo* repo, LayerSelectionClient* client);
	virtual ~LayerSelectorPanel();

	void moveCurrentLayer(bool up);

	int getSize();
	int getCurrentIndexExternal();
	void selectionChanged();
	void rowClicked(int row);

protected:
	Ref<LayerSelectionClient> client;

};