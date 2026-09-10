#include "EnvelopeMaterialization.h"

#include <algorithm>
#include <chrono>

#include <Curve/GuideCurveProvider.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include <Curve/Rasterization/Policies/Core/InterceptPolicies.h>
#include <Curve/Rasterization/Policies/Curves/CurvePolicies.h>
#include <Curve/Rasterization/Policies/Curves/WaveformBakePolicy.h>
#include <Curve/Rasterization/Policies/Envelope/EnvelopePolicies.h>
#include <Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h>

namespace Rasterization {
    namespace {
        constexpr int loopMinimumInterceptCount = 1;

        bool canLoop(const EnvelopeMaterializationResult& result) {
            return result.loopIndex >= 0;
        }

        bool hasReleaseCurve(
                const RenderResult& display,
                const EnvelopeMaterializationResult& result) {
            return EnvelopePlaybackPolicy().hasReleaseCurve(
                    display.intercepts,
                    result.sustainIndex);
        }

        float loopLength(
                const RenderResult& display,
                const EnvelopeMaterializationResult& result) {
            EnvelopePlaybackContext context;
            context.loopIndex = result.loopIndex;
            context.sustainIndex = result.sustainIndex;
            return EnvelopePlaybackPolicy().loopLength(display.intercepts, context);
        }

        EnvelopePaddingContext paddingContext(
                const RenderResult& display,
                const EnvelopeMaterializationResult& result,
                EnvelopePaddingContext::State state) {
            EnvelopePaddingContext context;
            context.state = state;
            context.loopMinSizeIcpts = loopMinimumInterceptCount;
            context.loopIndex = result.loopIndex;
            context.sustainIndex = result.sustainIndex;
            context.loopLength = loopLength(display, result);
            context.canLoop = canLoop(result);
            context.hasReleaseCurve = hasReleaseCurve(display, result);
            return context;
        }

        GuideCurveApplier guideCurveApplier(
                const RasterizationRequest& request,
                GuideCurveProvider* guideCurveProvider,
                const GuideCurveOffsetSeeds& offsetSeeds,
                RenderResult& display,
                VertCube::ReductionData& reduction) {
            GuideCurvePolicyContext context;
            context.guideCurveProvider = guideCurveProvider;
            context.reduction = &reduction;
            context.scalingMode = request.scalingMode;
            context.cyclic = request.cyclic;
            context.needsResorting = &display.needsResorting;
            context.noiseSeed = request.noiseSeed;
            context.offsetSeeds = &offsetSeeds;
            return GuideCurveApplier(context);
        }

        bool bakeWaveform(
                RenderResult& target,
                const RasterizationRequest& request,
                GuideCurveProvider* guideCurveProvider,
                const GuideCurveOffsetSeeds& offsetSeeds) {
            if (target.curves.size() < 2) {
                return false;
            }

            CurveResolutionPolicy::Context resolutionContext;
            resolutionContext.lowResCurves = request.lowResCurves;
            resolutionContext.integralSampling = request.integralSampling;
            resolutionContext.interpolateCurves = request.interpolateCurves;
            resolutionContext.paddingSize = target.paddingSize;
            CurveResolutionPolicy().apply(target.curves, resolutionContext);
            CurveWaveformPreparationPolicy().apply(target.curves);

            if (request.decoupleComponentDeforms) {
                target.guideCurveRegions.clear();
            }

            WaveformBakePolicy::Context context;
            context.lowResCurves = request.lowResCurves;
            context.decoupleComponentDfrms = request.decoupleComponentDeforms;
            context.noiseSeed = request.noiseSeed;
            context.morph = request.morph;
            context.guideCurveProvider = guideCurveProvider;
            context.guideCurveRegions = &target.guideCurveRegions;
            context.offsetSeeds = &offsetSeeds;

            return WaveformBakePolicy().build(
                    target.curves,
                    context,
                    [&target](int totalResolution) {
                        if (!target.placeWaveform(totalResolution)) {
                            return WaveformBufferRefs();
                        }
                        return WaveformBufferRefs(target.waveform);
                    });
        }

        bool waveformFits(const RenderResult& result) {
            int required = 0;
            for (int i = 0; i < (int) result.curves.size() - 1; ++i) {
                required += result.curves[(size_t) i].curveRes;
            }
            return required > 0 && result.waveformMemory.size() >= required * 5;
        }
    }

    bool EnvelopeMaterializationCapacity::isSupported() const {
        return intercepts > 0
                && intercepts <= maximumIntercepts
                && curves <= maximumCurves
                && guideCurveRegions <= maximumCurves
                && waveformSamples > 0
                && (size_t) waveformSamples <= maximumWaveformSamples;
    }

    EnvelopeMaterializationCapacity envelopeMaterializationCapacity(
            const EnvelopeMesh& mesh,
            const RasterizationRequest& request,
            GuideCurveProvider* guideCurveProvider) {
        EnvelopeMaterializationCapacity result;
        result.intercepts = (size_t) mesh.getNumCubes() + 1;
        result.curves = result.intercepts + 6;
        result.guideCurveRegions = result.curves;
        result.colorPoints = request.calcDepthDimensions
                ? (size_t) mesh.getNumCubes() * (size_t) request.dims.numHidden() * 2
                : 0;

        const int maximumCurveResolution = guideCurveProvider != nullptr
                ? GuideCurveProvider::tableSize
                : Curve::resolution / 2;
        result.waveformSamples = (int) ((result.curves - 1) * (size_t) maximumCurveResolution);
        return result;
    }

    EnvelopeMaterializationResult materializeEnvelope(
            const EnvelopeMesh& mesh,
            const RasterizationRequest& request,
            GuideCurveProvider* guideCurveProvider,
            const GuideCurveOffsetSeeds& offsetSeeds,
            EnvelopePaddingContext::State state,
            RenderResult& display,
            RenderResult& loop,
            VertCube::ReductionData& reduction) {
        EnvelopeMaterializationResult result;
        display.clear();
        loop.clear();

        if (mesh.getNumCubes() == 0) {
            return result;
        }

        GuideCurveApplier guideApplier = guideCurveApplier(
                request,
                guideCurveProvider,
                offsetSeeds,
                display,
                reduction);
        TrilinearMeshSlicer().sliceMesh(
                const_cast<EnvelopeMesh*>(&mesh),
                request,
                0.f,
                guideApplier,
                display,
                reduction);

        const auto markers = EnvelopeMarkerPolicy().evaluate(
                display.intercepts,
                &mesh,
                loopMinimumInterceptCount);
        result.loopIndex = markers.loopIndex;
        result.sustainIndex = markers.sustainIndex;

        EnvelopeSustainPointContext sustainContext;
        sustainContext.sustainIndex = result.sustainIndex;
        sustainContext.addFloorPoint = request.scalingMode != PointScalingMode::Bipolar;
        display.needsResorting |= EnvelopeSustainPointPolicy().apply(
                display.intercepts,
                sustainContext);
        InterceptSortPolicy(&display.needsResorting).sortIfNeeded(display.intercepts);

        switch (InterceptDegeneracyPolicy().classify(display.intercepts.size())) {
            case InterceptDegeneracyAction::CleanUp:
                display.clear();
                return result;
            case InterceptDegeneracyAction::MarkUnsampleable:
                display.curves.clear();
                display.waveform.nullify();
                return result;
            case InterceptDegeneracyAction::Continue:
                break;
        }

        InterceptPaddingFlagPolicy().apply(display.intercepts);
        const EnvelopePaddingContext displayContext = paddingContext(display, result, state);
        if (!EnvelopePaddingPolicy().buildDisplayPadding(
                display.intercepts,
                display.curves,
                displayContext)) {
            display.waveform.nullify();
            return result;
        }
        display.sampleable = bakeWaveform(
                display,
                request,
                guideCurveProvider,
                offsetSeeds);

        if (canLoop(result)) {
            loop.intercepts = display.intercepts;
            loop.paddingSize = display.paddingSize;
            EnvelopePaddingContext loopContext = paddingContext(
                    display,
                    result,
                    EnvelopePaddingContext::Looping);
            if (EnvelopePaddingPolicy().buildRenderPadding(
                    loop.intercepts,
                    loop.curves,
                    loopContext)) {
                loop.sampleable = bakeWaveform(
                        loop,
                        request,
                        guideCurveProvider,
                        offsetSeeds);
            }
        }

        result.capacityFailure = !waveformFits(display)
                || (!loop.curves.empty() && !waveformFits(loop));
        result.sampleable = display.sampleable && !result.capacityFailure;
        return result;
    }

    bool RealtimeEnvelopeMaterializer::prepare(const RealtimeEnvelopePlan& plan) {
        if (plan.mesh == nullptr || !plan.capacity.isSupported()) {
            return false;
        }

        if (!prepareSlot(slots[0], plan.capacity)
                || !prepareSlot(slots[1], plan.capacity)) {
            return false;
        }

        activePlan = plan;
        activeSlot = 0;
        hasActiveResult = false;
        instrumentation = {};
        return true;
    }

    bool RealtimeEnvelopeMaterializer::materialize(float red, float blue) {
        ++instrumentation.attempts;
        const auto started = std::chrono::steady_clock::now();
        const int inactiveSlot = 1 - activeSlot;
        Slot& target = slots[inactiveSlot];
        const auto displayInterceptCapacity = target.display.intercepts.capacity();
        const auto displayCurveCapacity = target.display.curves.capacity();
        const auto displayGuideCapacity = target.display.guideCurveRegions.capacity();
        const auto displayColorCapacity = target.display.colorPoints.capacity();
        const auto loopInterceptCapacity = target.loop.intercepts.capacity();
        const auto loopCurveCapacity = target.loop.curves.capacity();
        const auto loopGuideCapacity = target.loop.guideCurveRegions.capacity();
        RasterizationRequest request = activePlan.request;
        request.morph = MorphPosition(0.f, red, blue);
        target.metadata = materializeEnvelope(
                *activePlan.mesh,
                request,
                activePlan.guideCurveProvider,
                activePlan.offsetSeeds,
                EnvelopePaddingContext::NormalState,
                target.display,
                target.loop,
                target.reduction);
        target.metadata.capacityFailure |=
                target.display.intercepts.size() > activePlan.capacity.intercepts
                || target.display.curves.size() > activePlan.capacity.curves
                || target.display.guideCurveRegions.size()
                        > activePlan.capacity.guideCurveRegions
                || target.display.colorPoints.size() > activePlan.capacity.colorPoints
                || target.loop.intercepts.size() > activePlan.capacity.intercepts
                || target.loop.curves.size() > activePlan.capacity.curves
                || target.loop.guideCurveRegions.size()
                        > activePlan.capacity.guideCurveRegions
                || target.display.intercepts.capacity() != displayInterceptCapacity
                || target.display.curves.capacity() != displayCurveCapacity
                || target.display.guideCurveRegions.capacity() != displayGuideCapacity
                || target.display.colorPoints.capacity() != displayColorCapacity
                || target.loop.intercepts.capacity() != loopInterceptCapacity
                || target.loop.curves.capacity() != loopCurveCapacity
                || target.loop.guideCurveRegions.capacity() != loopGuideCapacity;
        const auto finished = std::chrono::steady_clock::now();
        const uint64_t elapsed = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(
                finished - started).count();
        updateInstrumentation(target, elapsed);

        if (!target.metadata.sampleable || target.metadata.capacityFailure) {
            ++instrumentation.failures;
            return false;
        }

        activeSlot = inactiveSlot;
        hasActiveResult = true;
        return true;
    }

    PreparedEnvelopePlaybackView RealtimeEnvelopeMaterializer::preparedPlaybackView() const {
        const Slot& slot = slots[activeSlot];
        return {
                slot.display,
                slot.loop,
                slot.metadata.loopIndex,
                slot.metadata.sustainIndex,
                activePlan.request.scalingMode,
                activePlan.guideCurveProvider,
                activePlan.request.noiseSeed
        };
    }

    bool RealtimeEnvelopeMaterializer::prepareSlot(
            Slot& slot,
            const EnvelopeMaterializationCapacity& capacity) {
        slot.display.intercepts.reserve(capacity.intercepts);
        slot.display.curves.reserve(capacity.curves);
        slot.display.guideCurveRegions.reserve(capacity.guideCurveRegions);
        slot.display.colorPoints.reserve(capacity.colorPoints);
        slot.display.waveformMemory.ensureSize(capacity.waveformSamples * 5);
        slot.display.fixedWaveformCapacity = true;

        slot.loop.intercepts.reserve(capacity.intercepts);
        slot.loop.curves.reserve(capacity.curves);
        slot.loop.guideCurveRegions.reserve(capacity.guideCurveRegions);
        slot.loop.colorPoints.reserve(capacity.colorPoints);
        slot.loop.waveformMemory.ensureSize(capacity.waveformSamples * 5);
        slot.loop.fixedWaveformCapacity = true;
        return slot.display.waveformMemory.size() >= capacity.waveformSamples * 5
                && slot.loop.waveformMemory.size() >= capacity.waveformSamples * 5;
    }

    void RealtimeEnvelopeMaterializer::updateInstrumentation(
            const Slot& slot,
            uint64_t elapsedNanoseconds) {
        instrumentation.lastElapsedNanoseconds = elapsedNanoseconds;
        instrumentation.maximumElapsedNanoseconds = std::max(
                instrumentation.maximumElapsedNanoseconds,
                elapsedNanoseconds);
        instrumentation.highWater.intercepts = std::max(
                instrumentation.highWater.intercepts,
                std::max(slot.display.intercepts.size(), slot.loop.intercepts.size()));
        instrumentation.highWater.curves = std::max(
                instrumentation.highWater.curves,
                std::max(slot.display.curves.size(), slot.loop.curves.size()));
        instrumentation.highWater.guideCurveRegions = std::max(
                instrumentation.highWater.guideCurveRegions,
                std::max(
                        slot.display.guideCurveRegions.size(),
                        slot.loop.guideCurveRegions.size()));
        instrumentation.highWater.colorPoints = std::max(
                instrumentation.highWater.colorPoints,
                slot.display.colorPoints.size());
        instrumentation.highWater.waveformSamples = std::max(
                instrumentation.highWater.waveformSamples,
                std::max(
                        slot.display.waveform.waveX.size(),
                        slot.loop.waveform.waveX.size()));
    }
}
