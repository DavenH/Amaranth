#pragma once

#include "JuceHeader.h"
#include <UI/Layout/ConstantSizeComponent.h>
#include <UI/Widgets/IconButton.h>

class LayerUpDownMover :
        public ConstantSizeComponent {
	JUCE_LEAK_DETECTOR(LayerAddRemover)

public:
	IconButton up;
	IconButton down;

	LayerUpDownMover(Button::Listener* listener, SingletonRepo* repo) :
		ConstantSizeComponent(36),
		up(3, 1, listener, repo, "Move current layer up"),
        down(4, 1, listener, repo, "Move current layer down") {
        addAndMakeVisible(&up);
		addAndMakeVisible(&down);

		up.setExpandedSize(16);
		up.setCollapsedSize(16);

		down.setExpandedSize(16);
		down.setCollapsedSize(16);
	}

    void resized() {
		up.setBounds(0, 0, getWidth(), getHeight() / 2);
		down.setBounds(0, getHeight() / 2, getWidth(), getHeight() / 2);
	}
};