#include <xutility>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <UI/IConsole.h>
#include <UI/MiscGraphics.h>
#include <Util/Arithmetic.h>

#include "LayerSelectionClient.h"
#include "LayerSelectorPanel.h"


LayerSelectorPanel::LayerSelectorPanel(SingletonRepo* repo, LayerSelectionClient* client) :
		SelectorPanel(repo), client(client) {
}


LayerSelectorPanel::~LayerSelectorPanel() {
}


void LayerSelectorPanel::rowClicked(int row) {
    getObj(MeshLibrary).getGroup(client->getLayerType()).current = row;
}


void LayerSelectorPanel::selectionChanged() {
    getObj(MeshLibrary).getLayer(client->getLayerType(), currentIndex);
    client->layerChanged();
}


int LayerSelectorPanel::getCurrentIndexExternal() {
    return getObj(MeshLibrary).getGroup(client->getLayerType()).current;
}


int LayerSelectorPanel::getSize() {
    return getObj(MeshLibrary).getGroup(client->getLayerType()).size();
}


void LayerSelectorPanel::moveCurrentLayer(bool up) {
    MeshLibrary::LayerGroup& group = getObj(MeshLibrary).getGroup(client->getLayerType());
    int size = group.size();

    if (currentIndex == 0 && !up || currentIndex == size - 1 && up)
        return;

    int movement = up ? 1 : -1;

    iter_swap(group.layers.begin() + currentIndex, group.layers.begin() + currentIndex + movement);
    currentIndex += movement;

    rowClicked(currentIndex);
    repaint();

    client->moveLayerProperties(currentIndex - movement, currentIndex);
    client->layerChanged();
}
