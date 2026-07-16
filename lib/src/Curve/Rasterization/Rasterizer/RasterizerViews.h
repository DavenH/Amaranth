#pragma once

#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Curve/Rasterization/WaveformBuffers.h>

#include "RasterizerData.h"

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
            if (!isSampleable()) {
                dest.zero();
                return;
            }
            WaveformSampler::sampleAtIntervals(buffers(), intervals, dest);
        }

        template<typename T>
        T sampleWithInterval(Buffer<float> dest, T delta, T phase) const {
            if (!isSampleable()) {
                dest.zero();
                return phase;
            }
            return WaveformSampler::sampleWithInterval(buffers(), dest, delta, phase);
        }

        float samplePerfectly(double delta, Buffer<float> dest, double phase) const {
            if (!isSampleable()) {
                dest.zero();
                return (float) phase;
            }
            return WaveformSampler::samplePerfectly(buffers(), delta, dest, phase);
        }

        int initialIndex() const {
            return waveform.zeroIndex;
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

        explicit SnapshotView(const RasterizerData& data) :
                data(&data)
            ,   lock(&data.lock) {
            lock->enter();
        }

        SnapshotView(const SnapshotView&) = delete;
        SnapshotView& operator=(const SnapshotView&) = delete;

        SnapshotView(SnapshotView&& other) noexcept :
                data(other.data)
            ,   lock(other.lock) {
            other.data = nullptr;
            other.lock = nullptr;
        }

        SnapshotView& operator=(SnapshotView&& other) noexcept {
            release();
            data = other.data;
            lock = other.lock;
            other.data = nullptr;
            other.lock = nullptr;
            return *this;
        }

        ~SnapshotView() {
            release();
        }

        const std::vector<Intercept>& intercepts() const & {
            return dataRef().intercepts;
        }
        const std::vector<Intercept>& intercepts() const && = delete;

        const std::vector<Curve>& curves() const & {
            return dataRef().curves;
        }
        const std::vector<Curve>& curves() const && = delete;

        const std::vector<ColorPoint>& colorPoints() const & {
            return dataRef().colorPoints;
        }
        const std::vector<ColorPoint>& colorPoints() const && = delete;

        Buffer<float> waveX() const & {
            return dataRef().waveX;
        }
        Buffer<float> waveX() const && = delete;

        Buffer<float> waveY() const & {
            return dataRef().waveY;
        }
        Buffer<float> waveY() const && = delete;

        int zeroIndex() const {
            return dataRef().zeroIndex;
        }

        int oneIndex() const {
            return dataRef().oneIndex;
        }

        int paddingSize() const {
            return dataRef().paddingSize;
        }

        bool wrapsVertices() const {
            return dataRef().wrapsVertices;
        }

        bool isSampleable() const {
            return dataRef().sampleable;
        }

    private:
        void release() {
            if (lock != nullptr) {
                lock->exit();
                lock = nullptr;
            }
        }

        const RasterizerData& dataRef() const {
            jassert(data != nullptr);
            return *data;
        }

        const RasterizerData* data {};
        CriticalSection* lock {};
    };
}
