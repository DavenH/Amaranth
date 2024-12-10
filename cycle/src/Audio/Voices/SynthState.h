#ifndef _SYNTHSTATE_H_
#define _SYNTHSTATE_H_

#include "../../Util/CycleEnums.h"

class SynthFlag
{
public:
	bool haveVolume;
	bool havePitch;
	bool haveTime;
	bool haveFFTPhase;
	bool haveOscPhase;
	bool haveFilter;
	bool haveCutoff;
	bool haveResonance;
	bool fadingOut;
	bool fadingIn;
	bool playing;
	bool looping;
	bool latencyFilling;
	bool releasing;
	bool sustaining;

public:

	SynthFlag()
	{
		reset();
	}

	void reset()
	{
		haveVolume 		= false;
		havePitch 		= false;
		haveTime 		= false;
		haveFFTPhase 	= false;
		haveOscPhase 	= false;
		haveFilter 		= false;
		haveCutoff 		= false;
		haveResonance 	= false;
		fadingOut 		= false;
		fadingIn 		= false;
		playing 		= false;
		looping 		= false;
		latencyFilling 	= false;
		releasing 		= false;
		sustaining 		= false;
	}
};


#endif

