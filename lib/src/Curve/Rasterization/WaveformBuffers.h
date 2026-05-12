#pragma once

#include "../../Array/Buffer.h"
#include "../../Array/ScopedAlloc.h"

namespace Rasterization {
    struct WaveformBuffers {
        Buffer<float> waveX;
        Buffer<float> waveY;
        Buffer<float> diffX;
        Buffer<float> slope;
        Buffer<float> area;

        int zeroIndex {};
        int oneIndex {};

        WaveformBuffers() = default;

        WaveformBuffers(int zeroIndex, int oneIndex) :
                zeroIndex(zeroIndex)
            ,   oneIndex(oneIndex) {
        }

        WaveformBuffers(
                Buffer<float> waveX,
                Buffer<float> waveY,
                Buffer<float> diffX,
                Buffer<float> slope,
                Buffer<float> area,
                int zeroIndex,
                int oneIndex) :
                waveX(waveX)
            ,   waveY(waveY)
            ,   diffX(diffX)
            ,   slope(slope)
            ,   area(area)
            ,   zeroIndex(zeroIndex)
            ,   oneIndex(oneIndex) {
        }

        void place(ScopedAlloc<float>& memory, int size) {
            memory.ensureSize(size * 5);
            waveX = memory.place(size);
            waveY = memory.place(size);
            diffX = memory.place(size);
            slope = memory.place(size);
            area = memory.place(size);
        }

        void nullify() {
            waveX.nullify();
            waveY.nullify();
            diffX.nullify();
            slope.nullify();
            area.nullify();
            zeroIndex = 0;
            oneIndex = 0;
        }

        bool isSampleable() const {
            return !waveX.empty() && !waveY.empty();
        }

        void copyFrom(const WaveformBuffers& source) {
            source.waveX.copyTo(waveX);
            source.waveY.copyTo(waveY);
            source.diffX.copyTo(diffX);
            source.slope.copyTo(slope);
            source.area.copyTo(area);
            zeroIndex = source.zeroIndex;
            oneIndex = source.oneIndex;
        }
    };

    struct WaveformBufferRefs {
        Buffer<float>* waveX {};
        Buffer<float>* waveY {};
        Buffer<float>* diffX {};
        Buffer<float>* slope {};
        Buffer<float>* area {};

        int* zeroIndex {};
        int* oneIndex {};

        WaveformBufferRefs() = default;

        explicit WaveformBufferRefs(WaveformBuffers& waveform) :
                WaveformBufferRefs(
                        waveform.waveX,
                        waveform.waveY,
                        waveform.diffX,
                        waveform.slope,
                        waveform.area,
                        waveform.zeroIndex,
                        waveform.oneIndex) {
        }

        WaveformBufferRefs(
                Buffer<float>& waveX,
                Buffer<float>& waveY,
                Buffer<float>& diffX,
                Buffer<float>& slope,
                Buffer<float>& area,
                int& zeroIndex,
                int& oneIndex) :
                waveX(&waveX)
            ,   waveY(&waveY)
            ,   diffX(&diffX)
            ,   slope(&slope)
            ,   area(&area)
            ,   zeroIndex(&zeroIndex)
            ,   oneIndex(&oneIndex) {
        }

        bool isBound() const {
            return waveX != nullptr
                && waveY != nullptr
                && diffX != nullptr
                && slope != nullptr
                && area != nullptr
                && zeroIndex != nullptr
                && oneIndex != nullptr;
        }

        bool isSampleable() const {
            return waveX != nullptr && !waveX->empty();
        }

        WaveformBuffers view() const {
            if (!isBound()) {
                return WaveformBuffers();
            }

            return WaveformBuffers(
                    *waveX,
                    *waveY,
                    *diffX,
                    *slope,
                    *area,
                    *zeroIndex,
                    *oneIndex);
        }

        void assignFrom(const WaveformBuffers& source) const {
            jassert(isBound());

            *waveX = source.waveX;
            *waveY = source.waveY;
            *diffX = source.diffX;
            *slope = source.slope;
            *area = source.area;
            *zeroIndex = source.zeroIndex;
            *oneIndex = source.oneIndex;
        }

        void copyFrom(const WaveformBuffers& source) const {
            jassert(isBound());

            source.waveX.copyTo(*waveX);
            source.waveY.copyTo(*waveY);
            source.diffX.copyTo(*diffX);
            source.slope.copyTo(*slope);
            source.area.copyTo(*area);
            *zeroIndex = source.zeroIndex;
            *oneIndex = source.oneIndex;
        }
    };
}
