#pragma once

#include "Sampling/WaveformSampler.h"
#include "WaveformBuffers.h"

struct RasterizerData;

namespace Rasterization {
    class SamplerView {
    public:
        SamplerView() = default;

        SamplerView(const WaveformBuffers& waveform, bool sampleable) :
                waveform(waveform)
            ,   sampleable(sampleable) {
        }

        bool isSampleable() const {
            return sampleable && WaveformSampler::isSampleable(waveform);
        }

        bool isSampleableAt(float x) const {
            return sampleable && WaveformSampler::isSampleableAt(waveform, x);
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
            return waveform;
        }

        WaveformBuffers waveform;
        bool sampleable {};
    };

    class SnapshotView {
    public:
        SnapshotView() = default;

        explicit SnapshotView(RasterizerData& data) :
                data(&data) {
        }

        RasterizerData& rasterizerData() const {
            jassert(data != nullptr);
            return *data;
        }

    private:
        RasterizerData* data {};
    };
}
