#pragma once
#include <App/MeshLibrary.h>

class LayerSelectionClient {
public:
    explicit LayerSelectionClient(SingletonRepo* repo);

    virtual ~LayerSelectionClient() = default;

    virtual void layerChanged() = 0;
    virtual int getLayerType() = 0;
    virtual void moveLayerProperties(int fromIndex, int toIndex) {}

protected:
//	LayerSelectorPanel layerSelector;

    friend class LayerSelectorPanel;
};