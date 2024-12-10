#ifndef _synthlookandfeel_h
#define _synthlookandfeel_h

#include "JuceHeader.h"
#include <UI/AmaranthLookAndFeel.h>

class SynthLookAndFeel :
		public AmaranthLookAndFeel {
public:
	SynthLookAndFeel(SingletonRepo* repo);

private:
	JUCE_LEAK_DETECTOR(SynthLookAndFeel)
};

#endif
