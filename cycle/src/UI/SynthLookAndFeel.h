#pragma once

#include "JuceHeader.h"
#include <UI/AmaranthLookAndFeel.h>

class SynthLookAndFeel :
		public AmaranthLookAndFeel {
public:
	SynthLookAndFeel(SingletonRepo* repo);

private:
	JUCE_LEAK_DETECTOR(SynthLookAndFeel)
};
