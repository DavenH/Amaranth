#pragma once

#include <App/SingletonAccessor.h>
#include "JuceHeader.h"

class Synthesizer :
		public Synthesiser
	,	public SingletonAccessor
{
public:
	enum ControllerNumbers
	{
		ModWheel = 1,

		// just a randomish number to separate it from
		// predefined midi controller values
		VolumeParam = 0x10000,
		OctaveParam,
		SpeedParam,
	};

	explicit Synthesizer(SingletonRepo* repo) : SingletonAccessor(repo, "Synthesizer") {}
	void handleController (int midiChannel, int controllerNumber, int controllerValue) override;
};
