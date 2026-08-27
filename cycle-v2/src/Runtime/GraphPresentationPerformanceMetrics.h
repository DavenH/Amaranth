#pragma once

#include <JuceHeader.h>

#include <array>

#include "Runtime/PerformanceDistribution.h"

namespace CycleV2 {

class GraphPresentationPerformanceMetrics final {
public:
    enum class Stage : uint8_t {
        SynchronousRefresh,
        QueueDelay,
        Worker,
        Configuration,
        PreviewAudio,
        PreviewExtraction,
        PublicationDelay,
        EndToEnd,
        Count
    };

    enum class Outcome : uint8_t {
        Requested,
        Published,
        NoWork,
        SupersededBeforeStart,
        StaleOrCancelled,
        Count
    };

    static constexpr size_t stageCount = static_cast<size_t>(Stage::Count);
    static constexpr size_t outcomeCount = static_cast<size_t>(Outcome::Count);
    using Distribution = PerformanceDistribution;

    using Clock = uint64_t (*)();

    explicit GraphPresentationPerformanceMetrics(Clock clock = defaultClock);

    uint64_t timestamp() const;
    void record(Stage stage, uint64_t elapsedMicroseconds);
    void record(Outcome outcome);
    void reset();

    std::array<Distribution, stageCount> stageSnapshot() const;
    std::array<uint64_t, outcomeCount> outcomeSnapshot() const;
    juce::var toVar() const;

    static const char* label(Stage stage);
    static const char* label(Outcome outcome);

private:
    static uint64_t defaultClock();
    Clock now;
    mutable juce::CriticalSection lock;
    std::array<Distribution, stageCount> stages;
    std::array<uint64_t, outcomeCount> outcomes {};
};

}
