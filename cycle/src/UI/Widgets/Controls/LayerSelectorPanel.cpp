#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>

#include "LayerSelectionClient.h"
#include "LayerSelectorPanel.h"

#include <Definitions.h>

LayerSelectorPanel::LayerSelectorPanel(SingletonRepo* repo, LayerSelectionClient* client) : SelectorPanel(repo),
    client(client) {
}

void LayerSelectorPanel::rowClicked(int row) {
    getObj(MeshLibrary).getLayerGroup(client->getLayerType()).current = row;
}

void LayerSelectorPanel::selectionChanged() {
    client->layerChanged();
}

int LayerSelectorPanel::getCurrentIndexExternal() {
    return getObj(MeshLibrary).getLayerGroup(client->getLayerType()).current;
}

int LayerSelectorPanel::getSize() {
    return getObj(MeshLibrary).getLayerGroup(client->getLayerType()).size();
}

void LayerSelectorPanel::moveCurrentLayer(bool up) {
    MeshLibrary::LayerGroup& group = getObj(MeshLibrary).getLayerGroup(client->getLayerType());
    int size = group.size();

    if (currentIndex == 0 && !up || currentIndex == size - 1 && up) {
        return;
    }

    int movement = up ? 1 : -1;
    int originalIndex = currentIndex;
    currentIndex += movement;

    rowClicked(currentIndex);
    repaint();

    getObj(MeshLibrary).moveLayer(client->getLayerType(), originalIndex, currentIndex);
    client->layerChanged();
}
