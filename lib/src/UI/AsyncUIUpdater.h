#pragma once

#include "JuceHeader.h"

class AsyncUIUpdater : public AsyncUpdater {
public:
	AsyncUIUpdater();

	virtual ~AsyncUIUpdater() override;

	void handleAsyncUpdate() override;

	virtual void doGlobalUIUpdate(bool force);
	virtual void reduceDetail();
	virtual void restoreDetail();

	[[nodiscard]] bool isDetailReduced() const;
	void triggerRestoreUpdate();
	void triggerRefreshUpdate();
	void triggerReduceUpdate();

	bool forceNextUIUpdate;
private:
	bool doRefresh;
	int reductionLevel;
};