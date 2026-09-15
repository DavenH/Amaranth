#include <chrono>

#include "SourceRenderPerformance.h"

namespace CycleDsp {

namespace {

uint64_t timestampNanoseconds() noexcept {
    return (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
}

}

void SourceRenderPerformance::add(const SourceRenderPerformance& other) noexcept {
    for (size_t index = 0; index < stageCount; ++index) {
        nanoseconds[index] += other.nanoseconds[index];
        operations[index] += other.operations[index];
    }
}

ScopedSourceRenderStage::ScopedSourceRenderStage(
        SourceRenderPerformance* counts,
        SourceRenderStage stage) noexcept :
        counts(counts)
    ,   stageIndex(static_cast<size_t>(stage))
    ,   startNanoseconds(counts == nullptr ? 0 : timestampNanoseconds()) {
}

ScopedSourceRenderStage::~ScopedSourceRenderStage() {
    finish();
}

void ScopedSourceRenderStage::finish() noexcept {
    if (counts != nullptr) {
        counts->nanoseconds[stageIndex] += timestampNanoseconds() - startNanoseconds;
        ++counts->operations[stageIndex];
        counts = nullptr;
    }
}

}
