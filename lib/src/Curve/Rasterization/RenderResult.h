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
    class WaveformView {
    public:
        WaveformView() = default;

        explicit WaveformView(const WaveformBuffers& waveform) :
                waveform(&waveform) {
        }

        Buffer<float> x() const { return waveform != nullptr ? waveform->waveX : Buffer<float>(); }
        Buffer<float> y() const { return waveform != nullptr ? waveform->waveY : Buffer<float>(); }
        Buffer<float> diffX() const { return waveform != nullptr ? waveform->diffX : Buffer<float>(); }
        Buffer<float> slope() const { return waveform != nullptr ? waveform->slope : Buffer<float>(); }
        Buffer<float> area() const { return waveform != nullptr ? waveform->area : Buffer<float>(); }

        int zeroIndex() const { return waveform != nullptr ? waveform->zeroIndex : 0; }
        int oneIndex() const { return waveform != nullptr ? waveform->oneIndex : 0; }
        bool empty() const { return waveform == nullptr || waveform->waveX.empty(); }
        const WaveformBuffers& buffers() const {
            static WaveformBuffers empty;
            return waveform != nullptr ? *waveform : empty;
        }

    private:
        const WaveformBuffers* waveform {};
    };

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

    class SnapshotView {
    public:
        SnapshotView() = default;

        SnapshotView(
                    const WaveformBuffers& waveform
                ,   const std::vector<Intercept>& intercepts
                ,   const std::vector<Curve>& curves
                ,   const std::vector<ColorPoint>& colorPoints) :
                waveform(&waveform)
            ,   intercepts(&intercepts)
            ,   curves(&curves)
            ,   colorPoints(&colorPoints) {
        }

        WaveformView waveformView() const { return waveform != nullptr ? WaveformView(*waveform) : WaveformView(); }
        const std::vector<Intercept>& getIntercepts() const { return intercepts != nullptr ? *intercepts : emptyIntercepts(); }
        const std::vector<Curve>& getCurves() const { return curves != nullptr ? *curves : emptyCurves(); }
        const std::vector<ColorPoint>& getColorPoints() const { return colorPoints != nullptr ? *colorPoints : emptyColorPoints(); }

    private:
        static const std::vector<Intercept>& emptyIntercepts() {
            static std::vector<Intercept> empty;
            return empty;
        }

        static const std::vector<Curve>& emptyCurves() {
            static std::vector<Curve> empty;
            return empty;
        }

        static const std::vector<ColorPoint>& emptyColorPoints() {
            static std::vector<ColorPoint> empty;
            return empty;
        }

        const WaveformBuffers* waveform {};
        const std::vector<Intercept>* intercepts {};
        const std::vector<Curve>* curves {};
        const std::vector<ColorPoint>* colorPoints {};
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

        WaveformView waveformView() const { return WaveformView(waveform); }
        SamplerView sampler() const { return SamplerView(waveform, &sampleable); }
        SnapshotView snapshot() const { return SnapshotView(waveform, intercepts, curves, colorPoints); }
    };
}
