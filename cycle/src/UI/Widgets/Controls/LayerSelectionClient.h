#pragma once
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