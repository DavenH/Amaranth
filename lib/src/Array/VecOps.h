#pragma once

#include "Buffer.h"

namespace VecOps {
    template<typename T> T* allocate(int size);
    // frees T*
    template<typename T> void deallocate(T*);

    // T = Float32, Float64
    template<typename T> void zero(T* src, int size);

    // T = Int8u, Int16s, Int32s, Float32, Float64, Complex32
    template<typename T> void copy(const T* src, T* dst, int size);

    // T = Float32, Float64, Complex32
    template<typename T> void add(Buffer<T> srcA, Buffer<T> srcB, Buffer<T> dst);
    template<typename T> void sub(Buffer<T> srcA, Buffer<T> srcB, Buffer<T> dst);
    template<typename T> void mul(Buffer<T> srcA, Buffer<T> srcB, Buffer<T> dst);
    template<typename T> void div(Buffer<T> numer, Buffer<T> denom, Buffer<T> dst);

    // moves A to B, where A may partially overlap B
    // T = Float32, Float64
    template<typename T> void move(Buffer<T> a, Buffer<T> b);

    // dst[i] = src[i + 1] - src[i], 0 <= i < n - 1
    // dst[n - 1] = dst[n - 2]
    // T = Float32, Float64
    template<typename T> void diff(Buffer<T> src, Buffer<T> dst);

    // dst[i] = src[src.size() - 1 - i]
    // T = Float32, Float64
    template<typename T> void flip(Buffer<T> src, Buffer<T> dst);

    // dst[i] = k / src[i]
    // T = Float32
    template<typename T> void divCRev(Buffer<T> src, T k, Buffer<T> dst);

    // dst[i] = k * src[i]
    // T = Float32, Float64
    template<typename T> void mul(Buffer<T> srcA, T k, Buffer<T> dst);

    // T = Float32
    template<typename T> void mul(T* srcA, T k, T* dst, int len);

    // dst[i] += src[i] * k
    // T = Float32
    template<typename T> void addProd(T* src, T k, T* dst, int len);

    /**
     * Places values into dst in the pattern A1 B1 A1 B2...An Bn
     * T = Float32, Float64
     */
    template<typename T> void interleave(Buffer<T> srcA, Buffer<T> srcB, Buffer<T> dst);

    /*
     * Rounds the floating point Buffer (T type) to output Buffer (S type, integer)
     * Rounds toward zero.
     * Supported rounding types:
     *  Float32 -> Int8u
     *  Float32 -> Int16s
     *  Float32 -> Int32s
     *  Float64 -> Int16s
     */
    template<typename T, typename S> void roundDown(Buffer<T> a, Buffer<S> b);

    /*
     * Converts the type from T to S, with T as a floating point type.
     * Supported conversions:
     * Float64 -> Float32
     * Float32 -> Float64
     */
    template<typename T, typename S> void convert(Buffer<T> src, Buffer<S> dst);

    /**
     * Convolves srcA with srcB, stores in dst.
     * Dst must be at least of length `srcA.size + srcB.size() - 1`
     * T = Float32
     */
    template<typename T> void conv(Buffer<T> srcA, Buffer<T> srcB, Buffer<T> dst);

    /**
     * kernel[i] = sin(relFreq * 2 * pi * i / kernel.size()) * blackman(i, kernel.size())
     * T = Float32
     */
    template<typename T> void sinc(Buffer<T> kernel, Buffer<T> window, T relFreq);

    /**
     * Apply Finite impulse response filter on the src buffer.
     * Uses a 32-point sinc filter.

     * @param src
     * @param dst
     * @param relFreq the relative frequency of the cutoff
     * @param trim If 'trim' is true, dst is assumed to be the same length as src
     *             Otherwise, dst must be of length at least src.size() + 32.
     * T = Float32
     */
    template<typename T> void fir(const Buffer<T>& src, Buffer<T> dst, T relFreq, bool trim = false);
};
