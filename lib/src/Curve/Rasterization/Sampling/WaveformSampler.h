#pragma once

#include "../WaveformBuffers.h"
#include "../../../Util/Arithmetic.h"

namespace Rasterization {
    class WaveformSampler {
    public:
        static bool isSampleable(const WaveformBuffers& waveform) {
            return !waveform.waveX.empty();
        }

        static bool isSampleableAt(const WaveformBuffers& waveform, float x) {
            return !waveform.waveX.empty()
                && waveform.waveX.front() <= x
                && waveform.waveX.back() > x;
        }

        static float sampleAt(const WaveformBuffers& waveform, bool unsampleable, double angle) {
            int currentIndex = Arithmetic::binarySearch(angle, waveform.waveX);
            return sampleAt(waveform, unsampleable, angle, currentIndex);
        }

        static float sampleAt(
                const WaveformBuffers& waveform,
                bool unsampleable,
                double angle,
                int& currentIndex) {
            if (unsampleable || (float) angle >= waveform.waveX.back() || (float) angle < waveform.waveX.front()) {
                jassertfalse;
                return angle;
            }

            if (currentIndex >= (int) waveform.waveX.size()) {
                currentIndex = 0;
            }

            if (currentIndex > 0) {
                while (angle < waveform.waveX[currentIndex - 1]) {
                    --currentIndex;
                    if (currentIndex == 0) {
                        currentIndex = waveform.waveX.size() - 1;
                    }
                }
            }

            while (angle >= waveform.waveX[currentIndex]) {
                ++currentIndex;

                if (currentIndex >= (int) waveform.waveX.size()) {
                    currentIndex = 0;
                }
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

            if (waveform.waveX.empty()) {
                dest.set(0.5f);

                return;
            }

            float* destPtr = dest.get();
            const float* intervalsPtr = intervals.get();

            jassert(waveform.waveX.front() < intervals.front()
                    && waveform.waveX.back() > intervals[dest.size() - 1]);

            int currentIndex = waveform.zeroIndex;

            for (int i = 0; i < (int) dest.size(); ++i) {
                while (intervalsPtr[i] >= waveform.waveX[currentIndex]) {
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

            double areaScale = 1. / delta;
            double halfDiff = 0.5 * delta;
            double accum = phase - halfDiff;

            jassert(waveform.waveX.front() <= accum);
            jassert(accum + (size + 0.5) * delta <= waveform.waveX.back());

            int currentIndex = waveform.zeroIndex;

            while (accum < waveform.waveX[currentIndex] && currentIndex > 0) {
                --currentIndex;
            }
            while (accum >= waveform.waveX[currentIndex + 1] && currentIndex < waveform.waveX.size()) {
                ++currentIndex;
            }

            jassert(waveform.waveX[currentIndex] <= accum && waveform.waveX[currentIndex + 1] >= accum);

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

                while (accum >= waveform.waveX[currentIndex]) {
                    ++currentIndex;
                }

                --currentIndex;
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

            if (phase > 0.5) {
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

            if (waveform.waveX.empty()) {
                buffer.set(0.5f);
                return 0;
            }

            auto lastAngle = (float) (size * delta + phase);

            jassert(waveform.waveX.front() < phase && waveform.waveX.back() > lastAngle);

            if (waveform.waveX.front() > phase || waveform.waveX.back() < lastAngle) {
                buffer.zero();
                phase += delta * size;
            } else {
                int currentIndex = jmax(0, waveform.zeroIndex - 1);

                while (phase < waveform.waveX[currentIndex] && currentIndex > 0) {
                    currentIndex--;
                }

                jassert(phase > waveform.waveX[currentIndex]);

                for (int i = 0; i < size; ++i) {
                    while (phase >= waveform.waveX[currentIndex + 1]) {
                        currentIndex++;
                    }

                    dest[i] = ((float) phase - waveform.waveX[currentIndex])
                            * waveform.slope[currentIndex]
                            + waveform.waveY[currentIndex];

                    phase += delta;
                }
            }

            if (phase > 0.5) {
                phase -= 1;
            }

            return phase;
        }
    };
}
