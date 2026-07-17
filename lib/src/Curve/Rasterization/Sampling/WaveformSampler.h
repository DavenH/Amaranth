#pragma once

#include "../WaveformBuffers.h"
#include "../../../Util/Arithmetic.h"

namespace Rasterization {
    class WaveformSampler {
    public:
        static bool isSampleable(const WaveformBuffers& waveform) {
            return waveform.waveX.size() >= 2
                && waveform.waveY.size() == waveform.waveX.size()
                && waveform.slope.size() >= waveform.waveX.size() - 1;
        }

        static bool isSampleableAt(const WaveformBuffers& waveform, float x) {
            return isSampleable(waveform)
                && waveform.waveX.front() <= x
                && waveform.waveX.back() > x;
        }

        static float sampleAt(const WaveformBuffers& waveform, bool unsampleable, double angle) {
            if (unsampleable || !isSampleableAt(waveform, (float) angle)) {
                return 0.f;
            }

            int currentIndex = Arithmetic::binarySearch(angle, waveform.waveX);
            return sampleAt(waveform, unsampleable, angle, currentIndex);
        }

        static float sampleAt(
                const WaveformBuffers& waveform,
                bool unsampleable,
                double angle,
                int& currentIndex) {
            if (unsampleable || !isSampleableAt(waveform, (float) angle)) {
                currentIndex = 0;
                return 0.f;
            }

            if (currentIndex < 0 || currentIndex >= (int) waveform.waveX.size()) {
                currentIndex = 0;
            }

            if (currentIndex > 0) {
                while (currentIndex > 0 && angle < waveform.waveX[currentIndex - 1]) {
                    --currentIndex;
                }
            }

            while (currentIndex < (int) waveform.waveX.size()
                    && angle >= waveform.waveX[currentIndex]) {
                ++currentIndex;
            }

            if (currentIndex == 0) {
                return waveform.waveY[0];
            }

            return (angle - waveform.waveX[currentIndex - 1]) * waveform.slope[currentIndex - 1]
                    + waveform.waveY[currentIndex - 1];
        }

        static void sampleAtIntervals(
                const WaveformBuffers& waveform,
                Buffer<float> intervals,
                Buffer<float> dest) {
            jassert(intervals.size() >= dest.size());

            if (dest.empty()) {
                return;
            }

            if (intervals.size() < dest.size() || !isSampleable(waveform)) {
                dest.zero();
                return;
            }

            for (int i = 0; i < dest.size(); ++i) {
                if (!isSampleableAt(waveform, intervals[i])
                        || (i > 0 && intervals[i] < intervals[i - 1])) {
                    dest.zero();
                    return;
                }
            }

            float* destPtr = dest.get();
            const float* intervalsPtr = intervals.get();

            int currentIndex = jlimit(1, waveform.waveX.size() - 1, waveform.zeroIndex);

            for (int i = 0; i < (int) dest.size(); ++i) {
                while (currentIndex > 1 && intervalsPtr[i] < waveform.waveX[currentIndex - 1]) {
                    --currentIndex;
                }
                while (currentIndex < waveform.waveX.size()
                        && intervalsPtr[i] >= waveform.waveX[currentIndex]) {
                    currentIndex++;
                }

                destPtr[i] = (intervalsPtr[i] - waveform.waveX[currentIndex - 1])
                        * waveform.slope[currentIndex - 1]
                        + waveform.waveY[currentIndex - 1];
            }
        }

        static float samplePerfectly(
                const WaveformBuffers& waveform,
                double delta,
                Buffer<float> buffer,
                double phase) {
            float* dest = buffer.get();
            int size = buffer.size();

            if (size == 0) {
                return (float) phase;
            }

            if (delta <= 0.
                    || !isSampleable(waveform)
                    || waveform.area.size() < waveform.waveX.size() - 1) {
                buffer.zero();
                return (float) phase;
            }

            double areaScale = 1. / delta;
            double halfDiff = 0.5 * delta;
            double accum = phase - halfDiff;

            const double finalBoundary = phase + (size - 0.5) * delta;
            if (accum < waveform.waveX.front() || finalBoundary > waveform.waveX.back()) {
                buffer.zero();
                return (float) phase;
            }

            int currentIndex = jlimit(0, waveform.waveX.size() - 2, waveform.zeroIndex);

            while (accum < waveform.waveX[currentIndex] && currentIndex > 0) {
                --currentIndex;
            }
            while (currentIndex + 1 < waveform.waveX.size() - 1
                    && accum >= waveform.waveX[currentIndex + 1]) {
                ++currentIndex;
            }

            if (accum < waveform.waveX.front()) {
                accum = waveform.waveX.front();
                currentIndex = 0;
            }

            int prevIdx = 0;
            int diffIdx = 0;
            float x = 0.f;
            float diffX = 0.f;

            for (int i = 0; i < size; ++i) {
                prevIdx = currentIndex;
                accum += delta;

                while (currentIndex < waveform.waveX.size()
                        && accum >= waveform.waveX[currentIndex]) {
                    ++currentIndex;
                }

                --currentIndex;
                currentIndex = jlimit(0, waveform.waveX.size() - 2, currentIndex);
                diffIdx = currentIndex - prevIdx;

                if (diffIdx == 0) {
                    dest[i] = ((accum - halfDiff - waveform.waveX[currentIndex])
                            * waveform.slope[currentIndex]
                            + waveform.waveY[currentIndex]);
                } else {
                    x = accum - delta;
                    dest[i] = 0.5f
                            * (waveform.slope[prevIdx] * (x - waveform.waveX[prevIdx])
                               + waveform.waveY[prevIdx]
                               + waveform.waveY[prevIdx + 1])
                            * (waveform.waveX[prevIdx + 1] - x);

                    for (int j = 0; j < diffIdx - 1; ++j) {
                        dest[i] += waveform.area[prevIdx + j + 1];
                    }

                    diffX = accum - waveform.waveX[currentIndex];
                    dest[i] += (0.5f * waveform.slope[currentIndex] * diffX + waveform.waveY[currentIndex]) * diffX;

                    dest[i] *= areaScale;
                }
            }

            phase = accum + halfDiff;

            while (phase > 0.5) {
                phase -= 1;
            }

            return phase;
        }

        template<typename T>
        static T sampleWithInterval(
                const WaveformBuffers& waveform,
                Buffer<float> buffer,
                T delta,
                T phase) {
            float* dest = buffer.get();
            int size = buffer.size();

            if (size == 0) {
                return phase;
            }

            const T lastAngle = phase + (size - 1) * delta;
            if (delta <= 0
                    || !isSampleable(waveform)
                    || phase < waveform.waveX.front()
                    || lastAngle >= waveform.waveX.back()) {
                buffer.zero();
                return phase;
            }

            int currentIndex = jlimit(0, waveform.waveX.size() - 2, waveform.zeroIndex);
            while (currentIndex > 0 && phase < waveform.waveX[currentIndex]) {
                --currentIndex;
            }
            while (currentIndex + 1 < waveform.waveX.size() - 1
                    && phase >= waveform.waveX[currentIndex + 1]) {
                ++currentIndex;
            }

            for (int i = 0; i < size; ++i) {
                while (currentIndex + 1 < waveform.waveX.size() - 1
                        && phase >= waveform.waveX[currentIndex + 1]) {
                    ++currentIndex;
                }

                dest[i] = ((float) phase - waveform.waveX[currentIndex])
                        * waveform.slope[currentIndex]
                        + waveform.waveY[currentIndex];

                phase += delta;
            }

            while (phase > 0.5) {
                phase -= 1;
            }

            return phase;
        }
    };
}
