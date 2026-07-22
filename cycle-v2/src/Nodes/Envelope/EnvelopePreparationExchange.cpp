#include <cstring>

#include "EnvelopePreparationExchange.h"

namespace CycleV2 {

namespace {

uint64_t packMorph(float red, float blue) {
    uint32_t packedRed;
    uint32_t packedBlue;
    std::memcpy(&packedRed, &red, sizeof(red));
    std::memcpy(&packedBlue, &blue, sizeof(blue));
    return static_cast<uint64_t>(packedBlue) << 32U | packedRed;
}

void unpackMorph(uint64_t packed, float& red, float& blue) {
    const uint32_t packedRed = static_cast<uint32_t>(packed);
    const uint32_t packedBlue = static_cast<uint32_t>(packed >> 32U);
    std::memcpy(&red, &packedRed, sizeof(red));
    std::memcpy(&blue, &packedBlue, sizeof(blue));
}

}

void LatestEnvelopePreparationRequest::reset() {
    sequence.store(0, std::memory_order_release);
    packedMorph.store(0, std::memory_order_release);
    noteSerial.store(0, std::memory_order_release);
    preparedGeneration = 0;
}

void LatestEnvelopePreparationRequest::publish(
        float redToPublish,
        float blueToPublish,
        uint64_t noteSerialToPublish) {
    const uint64_t writing = sequence.load(std::memory_order_relaxed) + 1;
    sequence.store(writing, std::memory_order_release);
    packedMorph.store(packMorph(redToPublish, blueToPublish), std::memory_order_release);
    noteSerial.store(noteSerialToPublish, std::memory_order_release);
    sequence.store(writing + 1, std::memory_order_release);
    ++publications;
}

bool LatestEnvelopePreparationRequest::latest(EnvelopePreparationRequest& result) const {
    constexpr int maximumReadAttempts = 3;
    for (int attempt = 0; attempt < maximumReadAttempts; ++attempt) {
        const uint64_t before = sequence.load(std::memory_order_acquire);
        if (before == 0) {
            return false;
        }
        if ((before & 1U) != 0) {
            continue;
        }
        const uint64_t morph = packedMorph.load(std::memory_order_acquire);
        const uint64_t note = noteSerial.load(std::memory_order_acquire);
        const uint64_t after = sequence.load(std::memory_order_acquire);
        if (before != after) {
            continue;
        }
        result.generation = after / 2;
        result.noteSerial = note;
        unpackMorph(morph, result.red, result.blue);
        return result.generation > preparedGeneration;
    }
    return false;
}

bool LatestEnvelopePreparationRequest::isCurrent(uint64_t generation) const {
    const uint64_t current = sequence.load(std::memory_order_acquire);
    return (current & 1U) == 0 && current / 2 == generation;
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
    for (int i = 0; i < static_cast<int>(slots.size()); ++i) {
        if (i != published && i != active) {
            destination = i;
            break;
        }
    }
    if (destination < 0) {
        return false;
    }

    slots[static_cast<size_t>(destination)] = {
            std::move(configuration), generation, noteSerial };
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
    const auto& slot = slots[static_cast<size_t>(slotIndex)];
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
    return slot >= 0 ? slots[static_cast<size_t>(slot)].configuration.get() : fallback;
}

std::shared_ptr<const EnvelopeConfiguration> PreparedEnvelopeExchange::activeOwnership(
        std::shared_ptr<const EnvelopeConfiguration> fallback) const {
    const int slot = activeSlot.load(std::memory_order_acquire);
    return slot >= 0 ? slots[static_cast<size_t>(slot)].configuration : std::move(fallback);
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
