#include "LayerAddRemover.h"
#include <UI/MiscGraphics.h>

LayerAddRemover::LayerAddRemover(Button::Listener* listener, SingletonRepo* repo, const String& layerString) :
        ConstantSizeComponent(36)
    ,	add	  (4, 7, listener, repo, "Add " + layerString)
    ,	remove(5, 7, listener, repo, "Remove " + layerString) {
    addAndMakeVisible(&add);
    addAndMakeVisible(&remove);

    add.setExpandedSize(16);
    add.setCollapsedSize(16);

    remove.setExpandedSize(16);
    remove.setCollapsedSize(16);
}

void LayerAddRemover::resized() {
    add.setBounds(0, 0, getWidth(), getHeight() / 2);
    remove.setBounds(0, getHeight() / 2, getWidth(), getHeight() / 2);
}

void LayerAddRemover::setLayerString(const String& layerString) {
    add.setMessages("Add " + layerString, {});
    remove.setMessages("Remove " + layerString, {});
}
