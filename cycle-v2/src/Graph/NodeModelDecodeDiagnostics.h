#pragma once

#include <atomic>
#include <cstdint>

namespace CycleV2 {

class NodeModelDecodeDiagnostics {
public:
    static void recordDecode() { decodeCount.fetch_add(1, std::memory_order_relaxed); }
    static void reset() { decodeCount.store(0, std::memory_order_relaxed); }
    static uint64_t count() { return decodeCount.load(std::memory_order_relaxed); }

private:
    static inline std::atomic<uint64_t> decodeCount {};
};

}
