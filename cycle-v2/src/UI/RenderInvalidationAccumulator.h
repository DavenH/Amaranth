#pragma once

#include <JuceHeader.h>

#include <cstdint>

namespace CycleV2 {

class RenderInvalidationTarget {
public:
    virtual ~RenderInvalidationTarget() = default;

    virtual uint32_t availableRenderInvalidations() const = 0;
    virtual void flushRenderInvalidations(uint32_t categories) = 0;
};

class RenderInvalidationAccumulator final : private juce::AsyncUpdater {
public:
    struct Diagnostics {
        uint64_t requests {};
        uint64_t scheduledFlushes {};
        uint64_t completedFlushes {};
        uint64_t categoryDispatches {};
    };

    explicit RenderInvalidationAccumulator(RenderInvalidationTarget& target);
    ~RenderInvalidationAccumulator() override;

    void request(uint32_t categories);
    void notifyAvailabilityChanged();
    void flushNowForTests();
    void resetDiagnostics();

    uint32_t pendingCategories() const;
    Diagnostics diagnostics() const;

private:
    void handleAsyncUpdate() override;
    void flushAvailableCategories();

    RenderInvalidationTarget& target;
    mutable juce::CriticalSection lock;
    uint32_t pending {};
    bool scheduled {};
    Diagnostics counters;
};

}
