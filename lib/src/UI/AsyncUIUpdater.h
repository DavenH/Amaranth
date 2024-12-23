#pragma once

#include "JuceHeader.h"

class AsyncUIUpdater : public juce::AsyncUpdater {
public:
    AsyncUIUpdater();

    ~AsyncUIUpdater() override = default;

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