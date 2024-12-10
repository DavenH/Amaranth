#pragma once

#include "JuceHeader.h"

class AsyncUIUpdater : public juce::AsyncUpdater {
public:
	AsyncUIUpdater();
	virtual ~AsyncUIUpdater();

	void handleAsyncUpdate();

	virtual void doGlobalUIUpdate(bool force);
	virtual void reduceDetail();
	virtual void restoreDetail();

	bool isDetailReduced();
	void triggerRestoreUpdate();
	void triggerRefreshUpdate();
	void triggerReduceUpdate();

	bool forceNextUIUpdate;
private:
	bool doRefresh;
	int reductionLevel;
};