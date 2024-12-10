#pragma once

#include "JuceHeader.h"
using namespace juce;

namespace Constants {
	enum {
		DeformTableSize		= 8192
	,	LogTension			= 50
	,	LowestMidiNote		= 20
	,	TitleBarHeight		= 24
	,	HighestMidiNote		= 127
	};

	static const String DocMagicCode = "Juce";
	static const String DocumentExt = "cyc";

	enum {
		DocSettingsDir
	,	DocumentName
	,	DocumentsDir
	,	numAppConstants
	};
}

class AppConstants  {
public:
	explicit AppConstants();
	~AppConstants() = default;

	void setConstant(int key, int value) 			{ values	.set(key, value); 	}
	void setConstant(int key, double value) 		{ realValues.set(key, value); 	}
	void setConstant(int key, const String& value) 	{ strValues	.set(key, value); 	}

	int getAppConstant(int key);
	double getRealAppConstant(int key);
	String getStringAppConstant(int key);

protected:

	HashMap<int, int> 	 values;
	HashMap<int, double> realValues;
	HashMap<int, String> strValues;
};
