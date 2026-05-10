#pragma once

#include <climits>
#include <vector>

#include "Sampling/GuideCurveSampler.h"
#include "Sampling/WaveformSampler.h"
#include "WaveformBuffers.h"
#include "../Curve.h"
#include "../Intercept.h"
#include "../../Array/ScopedAlloc.h"
#include "../../Obj/ColorPoint.h"

namespace Rasterization {
    class SamplerView {
    public:
        SamplerView() = default;

        SamplerView(const WaveformBuffers& waveform, const bool* sampleable) :
                waveform(&waveform)
            ,   sampleable(sampleable) {
        }

        bool isSampleable() const {
            return sampleable != nullptr && *sampleable && waveform != nullptr
                && WaveformSampler::isSampleable(*waveform);
        }

        bool isSampleableAt(float x) const {
            return sampleable != nullptr && *sampleable && waveform != nullptr
                && WaveformSampler::isSampleableAt(*waveform, x);
        }

        float sampleAt(double phase) const {
            return WaveformSampler::sampleAt(buffers(), !isSampleable(), phase);
        }

        float sampleAt(double phase, int& currentIndex) const {
            return WaveformSampler::sampleAt(buffers(), !isSampleable(), phase, currentIndex);
        }

        void sampleAtIntervals(Buffer<float> intervals, Buffer<float> dest) const {
            WaveformSampler::sampleAtIntervals(buffers(), intervals, dest);
        }

        template<typename T>
        T sampleWithInterval(Buffer<float> dest, T delta, T phase) const {
            return WaveformSampler::sampleWithInterval(buffers(), dest, delta, phase);
        }

        float samplePerfectly(double delta, Buffer<float> dest, double phase) const {
            return WaveformSampler::samplePerfectly(buffers(), delta, dest, phase);
        }

    private:
        const WaveformBuffers& buffers() const {
            static WaveformBuffers empty;
            return waveform != nullptr ? *waveform : empty;
        }

        const WaveformBuffers* waveform {};
        const bool* sampleable {};
    };

    struct RenderResult {
        std::vector<Intercept> intercepts;
        std::vector<Intercept> frontPadding;
        std::vector<Intercept> backPadding;
        std::vector<Curve> curves;
        std::vector<GuideCurveRegion> guideCurveRegions;
        std::vector<ColorPoint> colorPoints;

        ScopedAlloc<float> waveformMemory;
        WaveformBuffers waveform { 0, INT_MAX / 2 };

        int paddingSize { 1 };
        bool sampleable {};
        bool needsResorting {};

        void clear() {
            intercepts.clear();
            frontPadding.clear();
            backPadding.clear();
            curves.clear();
            guideCurveRegions.clear();
            colorPoints.clear();
            waveformMemory.clear();
            waveform.nullify();
            paddingSize = 1;
            sampleable = false;
            needsResorting = false;
        }

        SamplerView sampler() const { return SamplerView(waveform, &sampleable); }
    };
}
