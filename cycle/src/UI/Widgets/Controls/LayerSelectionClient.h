#pragma once
#include <App/MeshLibrary.h>

class LayerSelectionClient {
public:
    explicit LayerSelectionClient(SingletonRepo* repo);

    virtual ~LayerSelectionClient() = default;

    virtual void layerChanged() = 0;
    virtual int getLayerType() = 0;

protected:
//	LayerSelectorPanel layerSelector;

    friend class LayerSelectorPanel;
};