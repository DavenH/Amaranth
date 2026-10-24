#pragma once

#include <Array/Buffer.h>
#include <Curve/Rasterization/ScratchPositionPolicy.h>
#include <Obj/MorphPosition.h>

#include <algorithm>
#include <array>

#include "Runtime/MorphInputs.h"
#include "Runtime/SmoothedMorphPosition.h"

namespace CycleV2 {

class TrimeshMorphResolver {
public:
    void reset(
            const MorphPosition& fallback,
            bool snapDepthToFirstInputs = false) {
        smoothedMorph.reset(fallback);
        snapDepthOnNextResolve = snapDepthToFirstInputs;
        initialized = true;
    }

    MorphPosition resolve(
            const TrimeshMorphInputs& inputs,
            const MorphPosition& fallback,
            PortDomain outputDomain,
            int primaryAxis,
            size_t sampleOffset,
            size_t elapsedSamples,
            double sampleRate,
            bool scratchEnabled,
            int dimensionCount = 3) {
        if (!initialized) {
            reset(fallback);
        }

        const auto dimensions = MorphDimensions::trimesh(dimensionCount);
        if (dimensionCount == 0) {
            return capture(inputs, fallback);
        }
        const MorphPosition targets = inputs.resolve(fallback, dimensions, sampleOffset);
        const float time = targets.time;
        if (snapDepthOnNextResolve) {
            smoothedMorph.reset({
                    smoothedMorph.current().time.getCurrentValue(),
                    targets.red.getCurrentValue(),
                    targets.blue.getCurrentValue()
            });
            snapDepthOnNextResolve = false;
        }
        smoothedMorph.setTargets(targets);
        smoothedMorph.advance(elapsedSamples, sampleRate, dimensions);

        const MorphPosition morph = dimensions.constrain(
                smoothedMorph.current().withTime(time), fallback);
        const auto scratchDomain = domainFor(outputDomain);
        if (!dimensions.active(Vertex::Time) || !scratchEnabled
                || (!inputs.hasScratchOverride && inputs.scratch == nullptr)
                || !Rasterization::ScratchPositionPolicy::shouldApply(
                        scratchDomain, primaryAxis)) {
            return capture(inputs, morph);
        }
        return capture(inputs, Rasterization::ScratchPositionPolicy::resolve(
                morph,
                scratchDomain,
                primaryAxis,
                inputs.hasScratchOverride
                        ? jlimit(0.f, 1.f, inputs.scratchOverride)
                        : absoluteValue(
                                inputs.scratch,
                                sampleOffset,
                                morph.time.getCurrentValue())));
    }

    const MorphPosition& current() const { return smoothedMorph.current(); }

    static Rasterization::ScratchSourceDomain domainFor(PortDomain domain) {
        if (domain == PortDomain::TimeSignal) {
            return Rasterization::ScratchSourceDomain::Time;
        }
        if (domain == PortDomain::SpectralMagnitudeSignal
                || domain == PortDomain::SpectralPhaseSignal) {
            return Rasterization::ScratchSourceDomain::Spectral;
        }
        return Rasterization::ScratchSourceDomain::Unsupported;
    }

    static float absoluteValue(
            const SignalPayload* input,
            size_t sampleOffset,
            float fallback) {
        return MorphInputs::value(input, sampleOffset, fallback);
    }

    static float absoluteValue(
            const TrimeshMorphInputs& inputs,
            size_t axis,
            size_t sampleOffset,
            float fallback) {
        return inputs.value(axis, sampleOffset, fallback);
    }

private:
    static MorphPosition capture(const TrimeshMorphInputs& inputs, const MorphPosition& position) {
        if (inputs.playbackCursor != nullptr) {
            inputs.playbackCursor->setMorph(position);
        }
        return position;
    }

    bool initialized {};
    bool snapDepthOnNextResolve {};
    SmoothedMorphPosition smoothedMorph;
};

}
