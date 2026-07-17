#include "EnvelopePreparationExchange.h"

namespace CycleV2 {

void LatestEnvelopePreparationRequest::reset() {
    sequence.store(0, std::memory_order_release);
    preparedGeneration = 0;
}

void LatestEnvelopePreparationRequest::publish(
        float redToPublish,
        float blueToPublish,
        uint64_t noteSerialToPublish) {
    const uint64_t previous = sequence.load(std::memory_order_relaxed);
    const uint64_t next = (previous & ~uint64_t(1)) + 2;
    sequence.store(next - 1, std::memory_order_release);
    red.store(redToPublish, std::memory_order_relaxed);
    blue.store(blueToPublish, std::memory_order_relaxed);
    noteSerial.store(noteSerialToPublish, std::memory_order_relaxed);
    sequence.store(next, std::memory_order_release);
    ++publications;
}

bool LatestEnvelopePreparationRequest::latest(EnvelopePreparationRequest& result) const {
    const uint64_t firstSequence = sequence.load(std::memory_order_acquire);
    if (firstSequence == 0
            || (firstSequence & 1u) != 0
            || firstSequence <= preparedGeneration) {
        return false;
    }

    result.generation = firstSequence;
    result.noteSerial = noteSerial.load(std::memory_order_relaxed);
    result.red = red.load(std::memory_order_relaxed);
    result.blue = blue.load(std::memory_order_relaxed);
    return sequence.load(std::memory_order_acquire) == firstSequence;
}

bool LatestEnvelopePreparationRequest::isCurrent(uint64_t generation) const {
    return sequence.load(std::memory_order_acquire) == generation;
}

void LatestEnvelopePreparationRequest::markPrepared(uint64_t generation) {
    preparedGeneration = generation;
}

void LatestEnvelopePreparationRequest::recordStaleResult() {
    ++staleResults;
}

uint64_t LatestEnvelopePreparationRequest::publicationCount() const {
    return publications.load(std::memory_order_relaxed);
}

uint64_t LatestEnvelopePreparationRequest::staleResultCount() const {
    return staleResults.load(std::memory_order_relaxed);
}

void PreparedEnvelopeExchange::reset() {
    publishedSlot.store(-1, std::memory_order_release);
    activeSlot.store(-1, std::memory_order_release);
    adoptedGeneration.store(0, std::memory_order_release);
}

bool PreparedEnvelopeExchange::publish(
        std::shared_ptr<const EnvelopeConfiguration> configuration,
        uint64_t generation,
        uint64_t noteSerial) {
    const int published = publishedSlot.load(std::memory_order_acquire);
    const int active = activeSlot.load(std::memory_order_acquire);
    int destination = -1;
    for (int i = 0; i < (int) slots.size(); ++i) {
        if (i != published && i != active) {
            destination = i;
            break;
        }
    }
    if (destination < 0) {
        return false;
    }

    slots[(size_t) destination] = { std::move(configuration), generation, noteSerial };
    publishedSlot.store(destination, std::memory_order_release);
    ++preparations;
    return true;
}

PreparedEnvelopeExchange::Adoption PreparedEnvelopeExchange::adoptNewest(
        uint64_t noteSerial,
        bool dynamic) {
    const int slotIndex = publishedSlot.load(std::memory_order_acquire);
    if (slotIndex < 0) {
        return {};
    }
    const auto& slot = slots[(size_t) slotIndex];
    if (slot.configuration == nullptr
            || slot.generation <= adoptedGeneration.load(std::memory_order_relaxed)) {
        return {};
    }
    if (!dynamic && slot.noteSerial != noteSerial) {
        adoptedGeneration.store(slot.generation, std::memory_order_relaxed);
        ++staleResults;
        return { nullptr, false, true };
    }

    activeSlot.store(slotIndex, std::memory_order_release);
    adoptedGeneration.store(slot.generation, std::memory_order_relaxed);
    ++adoptions;
    return { slot.configuration.get(), true, false };
}

const EnvelopeConfiguration* PreparedEnvelopeExchange::active(
        const EnvelopeConfiguration* fallback) const {
    const int slot = activeSlot.load(std::memory_order_acquire);
    return slot >= 0 ? slots[(size_t) slot].configuration.get() : fallback;
}

std::shared_ptr<const EnvelopeConfiguration> PreparedEnvelopeExchange::activeOwnership(
        std::shared_ptr<const EnvelopeConfiguration> fallback) const {
    const int slot = activeSlot.load(std::memory_order_acquire);
    return slot >= 0 ? slots[(size_t) slot].configuration : std::move(fallback);
}

uint64_t PreparedEnvelopeExchange::preparationCount() const {
    return preparations.load(std::memory_order_relaxed);
}

uint64_t PreparedEnvelopeExchange::adoptionCount() const {
    return adoptions.load(std::memory_order_relaxed);
}

uint64_t PreparedEnvelopeExchange::staleResultCount() const {
    return staleResults.load(std::memory_order_relaxed);
}

}
