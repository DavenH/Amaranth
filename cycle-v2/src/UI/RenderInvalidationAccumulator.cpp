#include "RenderInvalidationAccumulator.h"

namespace CycleV2 {

RenderInvalidationAccumulator::RenderInvalidationAccumulator(
        RenderInvalidationTarget& targetToUse) :
        target(targetToUse) {
}

RenderInvalidationAccumulator::~RenderInvalidationAccumulator() {
    cancelPendingUpdate();
}

void RenderInvalidationAccumulator::request(uint32_t categories) {
    if (categories == 0) {
        return;
    }

    bool shouldSchedule = false;
    {
        const juce::ScopedLock scopedLock(lock);
        ++counters.requests;
        pending |= categories;
        if (!scheduled) {
            scheduled = true;
            ++counters.scheduledFlushes;
            shouldSchedule = true;
        }
    }
    if (shouldSchedule && juce::MessageManager::getInstanceWithoutCreating() != nullptr) {
        triggerAsyncUpdate();
    }
}

void RenderInvalidationAccumulator::notifyAvailabilityChanged() {
    bool shouldSchedule = false;
    {
        const juce::ScopedLock scopedLock(lock);
        if (pending != 0 && !scheduled) {
            scheduled = true;
            ++counters.scheduledFlushes;
            shouldSchedule = true;
        }
    }
    if (shouldSchedule && juce::MessageManager::getInstanceWithoutCreating() != nullptr) {
        triggerAsyncUpdate();
    }
}

void RenderInvalidationAccumulator::flushNowForTests() {
    cancelPendingUpdate();
    {
        const juce::ScopedLock scopedLock(lock);
        scheduled = false;
    }
    flushAvailableCategories();
}

void RenderInvalidationAccumulator::resetDiagnostics() {
    const juce::ScopedLock scopedLock(lock);
    counters = {};
}

RenderInvalidationAccumulator::Diagnostics RenderInvalidationAccumulator::diagnostics() const {
    const juce::ScopedLock scopedLock(lock);
    return counters;
}

uint32_t RenderInvalidationAccumulator::pendingCategories() const {
    const juce::ScopedLock scopedLock(lock);
    return pending;
}

void RenderInvalidationAccumulator::handleAsyncUpdate() {
    {
        const juce::ScopedLock scopedLock(lock);
        scheduled = false;
    }
    flushAvailableCategories();
}

void RenderInvalidationAccumulator::flushAvailableCategories() {
    const uint32_t availability = target.availableRenderInvalidations();
    uint32_t available {};
    {
        const juce::ScopedLock scopedLock(lock);
        available = pending & availability;
        pending &= ~available;
    }
    if (available == 0) {
        return;
    }

    uint32_t categories = available;
    uint64_t categoryCount {};
    while (categories != 0) {
        categoryCount += categories & 1u;
        categories >>= 1u;
    }
    {
        const juce::ScopedLock scopedLock(lock);
        ++counters.completedFlushes;
        counters.categoryDispatches += categoryCount;
    }
    target.flushRenderInvalidations(available);
}

}
