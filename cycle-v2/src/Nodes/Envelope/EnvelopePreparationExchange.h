#pragma once

#include <array>
#include <atomic>

#include "EnvelopeConfiguration.h"

namespace CycleV2 {

struct EnvelopePreparationRequest {
    uint64_t generation {};
    uint64_t noteSerial {};
    float red {};
    float blue {};
};

class LatestEnvelopePreparationRequest {
public:
    void reset();
    void publish(float red, float blue, uint64_t noteSerial);
    bool latest(EnvelopePreparationRequest& result) const;
    bool isCurrent(uint64_t generation) const;
    void markPrepared(uint64_t generation);
    void recordStaleResult();
    uint64_t publicationCount() const;
    uint64_t staleResultCount() const;

private:
    std::atomic<uint64_t> sequence {};
    std::atomic<uint64_t> packedMorph {};
    std::atomic<uint64_t> noteSerial {};
    std::atomic<uint64_t> publications {};
    std::atomic<uint64_t> staleResults {};
    uint64_t preparedGeneration {};
};

class PreparedEnvelopeExchange {
public:
    struct Adoption {
        const EnvelopeConfiguration* configuration {};
        bool adopted {};
        bool stale {};
    };

    void reset();
    bool publish(
            std::shared_ptr<const EnvelopeConfiguration> configuration,
            uint64_t generation,
            uint64_t noteSerial);
    Adoption adoptNewest(uint64_t noteSerial, bool dynamic);
    const EnvelopeConfiguration* active(const EnvelopeConfiguration* fallback) const;
    std::shared_ptr<const EnvelopeConfiguration> activeOwnership(
            std::shared_ptr<const EnvelopeConfiguration> fallback) const;

    uint64_t preparationCount() const;
    uint64_t adoptionCount() const;
    uint64_t staleResultCount() const;

private:
    struct Slot {
        std::shared_ptr<const EnvelopeConfiguration> configuration;
        uint64_t generation {};
        uint64_t noteSerial {};
    };

    std::array<Slot, 3> slots;
    std::atomic<int> publishedSlot { -1 };
    std::atomic<int> activeSlot { -1 };
    std::atomic<uint64_t> adoptedGeneration {};
    std::atomic<uint64_t> preparations {};
    std::atomic<uint64_t> adoptions {};
    std::atomic<uint64_t> staleResults {};
};

}
