#include "Runtime/GraphPresentationPerformanceMetrics.h"

namespace CycleV2 {

using namespace juce;

namespace {

size_t indexFor(GraphPresentationPerformanceMetrics::Stage stage) {
    return static_cast<size_t>(stage);
}

size_t indexFor(GraphPresentationPerformanceMetrics::Outcome outcome) {
    return static_cast<size_t>(outcome);
}

}

GraphPresentationPerformanceMetrics::GraphPresentationPerformanceMetrics(Clock clock) :
        now(clock) {
}

uint64_t GraphPresentationPerformanceMetrics::timestamp() const {
    return now();
}

void GraphPresentationPerformanceMetrics::record(
        Stage stage,
        uint64_t elapsedMicroseconds) {
    const ScopedLock scopedLock(lock);
    recordPerformanceSample(stages[indexFor(stage)], elapsedMicroseconds);
}

void GraphPresentationPerformanceMetrics::record(Outcome outcome) {
    const ScopedLock scopedLock(lock);
    ++outcomes[indexFor(outcome)];
}

void GraphPresentationPerformanceMetrics::reset() {
    const ScopedLock scopedLock(lock);
    stages = {};
    outcomes = {};
}

std::array<GraphPresentationPerformanceMetrics::Distribution,
        GraphPresentationPerformanceMetrics::stageCount>
GraphPresentationPerformanceMetrics::stageSnapshot() const {
    const ScopedLock scopedLock(lock);
    return stages;
}

std::array<uint64_t, GraphPresentationPerformanceMetrics::outcomeCount>
GraphPresentationPerformanceMetrics::outcomeSnapshot() const {
    const ScopedLock scopedLock(lock);
    return outcomes;
}

var GraphPresentationPerformanceMetrics::toVar() const {
    const auto stageValues = stageSnapshot();
    const auto outcomeValues = outcomeSnapshot();
    auto* root = new DynamicObject();

    auto* stageObject = new DynamicObject();
    for (size_t index = 0; index < stageCount; ++index) {
        const auto stage = static_cast<Stage>(index);
        stageObject->setProperty(
                label(stage),
                performanceDistributionToVar(stageValues[index]));
    }
    root->setProperty("stages", var(stageObject));

    auto* outcomeObject = new DynamicObject();
    for (size_t index = 0; index < outcomeCount; ++index) {
        const auto outcome = static_cast<Outcome>(index);
        outcomeObject->setProperty(label(outcome), (int64) outcomeValues[index]);
    }
    root->setProperty("outcomes", var(outcomeObject));
    return var(root);
}

const char* GraphPresentationPerformanceMetrics::label(Stage stage) {
    switch (stage) {
        case Stage::SynchronousRefresh: return "synchronousRefresh";
        case Stage::QueueDelay:         return "queueDelay";
        case Stage::Worker:             return "worker";
        case Stage::Configuration:      return "configuration";
        case Stage::PreviewAudio:       return "previewAudio";
        case Stage::PreviewExtraction:  return "previewExtraction";
        case Stage::PublicationDelay:   return "publicationDelay";
        case Stage::EndToEnd:           return "endToEnd";
        case Stage::Count:              break;
    }
    return "unknown";
}

const char* GraphPresentationPerformanceMetrics::label(Outcome outcome) {
    switch (outcome) {
        case Outcome::Requested:                return "requested";
        case Outcome::Published:                return "published";
        case Outcome::NoWork:                   return "noWork";
        case Outcome::SupersededBeforeStart:    return "supersededBeforeStart";
        case Outcome::StaleOrCancelled:         return "staleOrCancelled";
        case Outcome::Count:                    break;
    }
    return "unknown";
}

uint64_t GraphPresentationPerformanceMetrics::defaultClock() {
    return (uint64_t) (Time::getMillisecondCounterHiRes() * 1000.0);
}

}
