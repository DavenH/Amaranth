#ifndef _LAYERSELECTIONCLIENT_H_
#define _LAYERSELECTIONCLIENT_H_

#include "LayerSelectorPanel.h"
#include <App/MeshLibrary.h>

class LayerSelectionClient {
public:
	LayerSelectionClient(SingletonRepo* repo);
	virtual ~LayerSelectionClient();

	virtual void layerChanged() = 0;
	virtual int getLayerType() = 0;
	virtual void moveLayerProperties(int fromIndex, int toIndex) {}

protected:
//	LayerSelectorPanel layerSelector;

	friend class LayerSelectorPanel;
};

#endif
