#include "AsyncUIUpdater.h"

using namespace juce;

AsyncUIUpdater::AsyncUIUpdater() :
        doRefresh(false)
    ,	forceNextUIUpdate(false)
    ,	reductionLevel(0) {
}

void AsyncUIUpdater::doGlobalUIUpdate(bool force) {}

void AsyncUIUpdater::reduceDetail() {}

void AsyncUIUpdater::restoreDetail() {}

void AsyncUIUpdater::triggerReduceUpdate() {
    jassert(reductionLevel == 0);
    --reductionLevel;

    triggerAsyncUpdate();
}

void AsyncUIUpdater::triggerRestoreUpdate() {
//	jassert(reductionLevel < 0);

    ++reductionLevel;
    doRefresh = true;

    triggerAsyncUpdate();
}

void AsyncUIUpdater::triggerRefreshUpdate() {
    doRefresh = true;

    triggerAsyncUpdate();
}

void AsyncUIUpdater::handleAsyncUpdate() {
    if (reductionLevel < 0) {
        reduceDetail();
    } else if (reductionLevel > 0) {
        restoreDetail();
    }

    reductionLevel = 0;

    if (doRefresh) {
        doGlobalUIUpdate(forceNextUIUpdate);
    }

    forceNextUIUpdate = false;
}

bool AsyncUIUpdater::isDetailReduced() const {
    return reductionLevel > 0;
}
