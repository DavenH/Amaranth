#pragma once

#include <cstddef>
#include <cstdint>

#include <Curve/GuideCurveProvider.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/EnvelopePlaybackEngine.h>
#include <Curve/Rasterization/GuideCurveOffsetSeeds.h>
#include <Curve/Rasterization/Policies/Envelope/EnvelopePolicies.h>
#include <Curve/Rasterization/RasterizationRequest.h>
#include <Curve/Rasterization/RenderResult.h>

namespace Rasterization {
    struct EnvelopeMaterializationCapacity {
        static constexpr size_t maximumIntercepts = 4096;
        static constexpr size_t maximumCurves = maximumIntercepts + 6;
        static constexpr size_t maximumWaveformSamples = 1u << 20u;

        size_t intercepts {};
        size_t curves {};
        size_t guideCurveRegions {};
        size_t colorPoints {};
        int waveformSamples {};

        bool isSupported() const;
    };

    struct RealtimeEnvelopePlan {
        const EnvelopeMesh* mesh {};
        RasterizationRequest request;
        EnvelopeMaterializationCapacity capacity;
        GuideCurveProvider* guideCurveProvider {};
        GuideCurveOffsetSeeds offsetSeeds;
    };

    struct EnvelopeMaterializationResult {
        int loopIndex { -1 };
        int sustainIndex { -1 };
        bool sampleable {};
        bool capacityFailure {};
    };

    struct EnvelopeMaterializationHighWaterMarks {
        size_t intercepts {};
        size_t curves {};
        size_t guideCurveRegions {};
        size_t colorPoints {};
        int waveformSamples {};
    };

    struct RealtimeEnvelopeMaterializationDiagnostics {
        uint64_t attempts {};
        uint64_t failures {};
        uint64_t lastElapsedNanoseconds {};
        uint64_t maximumElapsedNanoseconds {};
        EnvelopeMaterializationHighWaterMarks highWater;
    };

    EnvelopeMaterializationCapacity envelopeMaterializationCapacity(
            const EnvelopeMesh& mesh,
            const RasterizationRequest& request,
            GuideCurveProvider* guideCurveProvider);

    EnvelopeMaterializationResult materializeEnvelope(
            const EnvelopeMesh& mesh,
            const RasterizationRequest& request,
            GuideCurveProvider* guideCurveProvider,
            const GuideCurveOffsetSeeds& offsetSeeds,
            EnvelopePaddingContext::State state,
            RenderResult& display,
            RenderResult& loop,
            VertCube::ReductionData& reduction);

    class RealtimeEnvelopeMaterializer {
    public:
        bool prepare(const RealtimeEnvelopePlan& plan);
        bool materialize(float red, float blue);

        PreparedEnvelopePlaybackView preparedPlaybackView() const;
        const RealtimeEnvelopeMaterializationDiagnostics& diagnostics() const {
            return instrumentation;
        }
        bool hasPreparedEnvelope() const { return hasActiveResult; }

    private:
        struct Slot {
            RenderResult display;
            RenderResult loop;
            VertCube::ReductionData reduction;
            EnvelopeMaterializationResult metadata;
        };

        static bool prepareSlot(Slot& slot, const EnvelopeMaterializationCapacity& capacity);
        void updateInstrumentation(const Slot& slot, uint64_t elapsedNanoseconds);

        Slot slots[2];
        int activeSlot {};
        bool hasActiveResult {};
        RealtimeEnvelopePlan activePlan;
        RealtimeEnvelopeMaterializationDiagnostics instrumentation;
    };
}
