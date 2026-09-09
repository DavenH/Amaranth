#pragma once

#include <Array/Buffer.h>
#include <Curve/Rasterization/ScratchPositionPolicy.h>
#include <Obj/MorphPosition.h>

#include <algorithm>
#include <array>

#include "Runtime/AudioProcessTypes.h"
#include "Runtime/SmoothedMorphPosition.h"

namespace CycleV2 {

struct TrimeshMorphInputs {
    std::array<const SignalPayload*, 3> absoluteMorph {};
    std::array<float, 3> absoluteOverrides {};
    std::array<bool, 3> hasAbsoluteOverride {};
    const SignalPayload* scratch {};
};

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
            bool scratchEnabled) {
        if (!initialized) {
            reset(fallback);
        }

        const float time = absoluteValue(
                inputs,
                0,
                sampleOffset,
                fallback.time.getCurrentValue());
        const MorphPosition targets {
                time,
                absoluteValue(inputs, 1, sampleOffset,
                        fallback.red.getCurrentValue()),
                absoluteValue(inputs, 2, sampleOffset,
                        fallback.blue.getCurrentValue())
        };
        if (snapDepthOnNextResolve) {
            smoothedMorph.reset({
                    smoothedMorph.current().time.getCurrentValue(),
                    targets.red.getCurrentValue(),
                    targets.blue.getCurrentValue()
            });
            snapDepthOnNextResolve = false;
        }
        smoothedMorph.setTargets(targets);
        smoothedMorph.advance(elapsedSamples, sampleRate);

        const MorphPosition morph = smoothedMorph.current().withTime(time);
        const auto scratchDomain = domainFor(outputDomain);
        if (!scratchEnabled
                || inputs.scratch == nullptr
                || !Rasterization::ScratchPositionPolicy::shouldApply(
                        scratchDomain, primaryAxis)) {
            return morph;
        }
        return Rasterization::ScratchPositionPolicy::resolve(
                morph,
                scratchDomain,
                primaryAxis,
                absoluteValue(
                        inputs.scratch,
                        sampleOffset,
                        morph.time.getCurrentValue()));
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
        if (input == nullptr || input->block.samples.empty()) {
            return fallback;
        }
        const size_t index = std::min(
                sampleOffset,
                input->block.samples.size() - 1);
        return jlimit(0.f, 1.f, input->block.samples[index]);
    }

    static float absoluteValue(
            const TrimeshMorphInputs& inputs,
            size_t axis,
            size_t sampleOffset,
            float fallback) {
        if (inputs.hasAbsoluteOverride[axis]) {
            return jlimit(0.f, 1.f, inputs.absoluteOverrides[axis]);
        }
        return absoluteValue(inputs.absoluteMorph[axis], sampleOffset, fallback);
    }

private:
    bool initialized {};
    bool snapDepthOnNextResolve {};
    SmoothedMorphPosition smoothedMorph;
};

}
