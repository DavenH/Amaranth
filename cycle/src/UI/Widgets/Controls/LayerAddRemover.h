#ifndef _layeraddremover_h
#define _layeraddremover_h

#include "JuceHeader.h"
#include <UI/Layout/ConstantSizeComponent.h>
#include <UI/Widgets/IconButton.h>

class LayerAddRemover :
	public ConstantSizeComponent {
public:
	LayerAddRemover(Button::Listener* listener, SingletonRepo* repo, const String& layerString);
	void setLayerString(const String& layerString);
	void resized();

	IconButton add;
	IconButton remove;
private:
	JUCE_LEAK_DETECTOR(LayerAddRemover)
};

#endif
