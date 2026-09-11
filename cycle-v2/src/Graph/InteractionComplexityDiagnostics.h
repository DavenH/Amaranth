#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace CycleV2 {

struct InteractionComplexityCounts {
    uint64_t graphCopies {};
    uint64_t audioSamplesCopied {};
    uint64_t meshCopies {};
    uint64_t meshVerticesCopied {};
    uint64_t meshCubesCopied {};
    uint64_t modelSerializations {};
    uint64_t editorStateComparisons {};
    uint64_t nodeLinearScans {};
    uint64_t parameterLinearScans {};
    uint64_t assignmentLinearScans {};
};

class InteractionComplexityDiagnostics {
public:
    static void reset() {
        graphCopies.store(0, std::memory_order_relaxed);
        audioSamplesCopied.store(0, std::memory_order_relaxed);
        meshCopies.store(0, std::memory_order_relaxed);
        meshVerticesCopied.store(0, std::memory_order_relaxed);
        meshCubesCopied.store(0, std::memory_order_relaxed);
        modelSerializations.store(0, std::memory_order_relaxed);
        editorStateComparisons.store(0, std::memory_order_relaxed);
        nodeLinearScans.store(0, std::memory_order_relaxed);
        parameterLinearScans.store(0, std::memory_order_relaxed);
        assignmentLinearScans.store(0, std::memory_order_relaxed);
    }

    static InteractionComplexityCounts counts() {
        return {
                graphCopies.load(std::memory_order_relaxed),
                audioSamplesCopied.load(std::memory_order_relaxed),
                meshCopies.load(std::memory_order_relaxed),
                meshVerticesCopied.load(std::memory_order_relaxed),
                meshCubesCopied.load(std::memory_order_relaxed),
                modelSerializations.load(std::memory_order_relaxed),
                editorStateComparisons.load(std::memory_order_relaxed),
                nodeLinearScans.load(std::memory_order_relaxed),
                parameterLinearScans.load(std::memory_order_relaxed),
                assignmentLinearScans.load(std::memory_order_relaxed)
        };
    }

    static void recordGraphCopy() { ++graphCopies; }
    static void recordAudioSampleCopy(size_t count) { audioSamplesCopied += count; }
    static void recordMeshCopy(size_t vertices, size_t cubes) {
        ++meshCopies;
        meshVerticesCopied += vertices;
        meshCubesCopied += cubes;
    }
    static void recordModelSerialization() { ++modelSerializations; }
    static void recordEditorStateComparison() { ++editorStateComparisons; }
    static void recordNodeLinearScan() { ++nodeLinearScans; }
    static void recordParameterLinearScan() { ++parameterLinearScans; }
    static void recordAssignmentLinearScan() { ++assignmentLinearScans; }

private:
    static inline std::atomic<uint64_t> graphCopies {};
    static inline std::atomic<uint64_t> audioSamplesCopied {};
    static inline std::atomic<uint64_t> meshCopies {};
    static inline std::atomic<uint64_t> meshVerticesCopied {};
    static inline std::atomic<uint64_t> meshCubesCopied {};
    static inline std::atomic<uint64_t> modelSerializations {};
    static inline std::atomic<uint64_t> editorStateComparisons {};
    static inline std::atomic<uint64_t> nodeLinearScans {};
    static inline std::atomic<uint64_t> parameterLinearScans {};
    static inline std::atomic<uint64_t> assignmentLinearScans {};
};

}
