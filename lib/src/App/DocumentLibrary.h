#pragma once

#include <set>
#include "Doc/DocumentDetails.h"
#include "JuceHeader.h"
#include "SingletonAccessor.h"

using std::set;
using namespace juce;

class DocumentLibrary :
		public SingletonAccessor
	,	public AsyncUpdater {
public:
	enum { NullPresetIndex = -1 };

	explicit DocumentLibrary(SingletonRepo*);
	~DocumentLibrary() override = default;

	void handleAsyncUpdate() override;
	void init() override;
	void load(int index);
	void readDocuments(const String& dir);
	void triggerAsyncLoad(int index);

	void writeSettingsFile();
	bool readSettingsFile();

	String getProgramName(int index);

	int getSize() 							{ return allDocs.size(); 		}
	int getCurrentIndex() const 			{ return currentPresetIndex; 	}
	void setShouldOpenDefault(bool should) 	{ shouldOpenDefault = should; 	}

private:
	bool settingsArePending, shouldOpenDefault;
	int currentPresetIndex, pendingPresetIndex;

	Array<DocumentDetails> allDocs;
	HashMap<int, float> ratingsMap;
	set<int> dismissedSet, downloadedSet;
};
