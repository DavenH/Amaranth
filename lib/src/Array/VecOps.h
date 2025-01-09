#pragma once

#include "Buffer.h"

namespace VecOps {
    template<typename T> T* allocate(int size);
    template<typename T> void deallocate(T*);

    template<typename T> void zero(T* src, int size);
    template<typename T> void copy(const T* src, T* dst, int size);

    template<typename T> void add(Buffer<T> srcA, Buffer<T> srcB, Buffer<T> dst);
    template<typename T> void sub(Buffer<T> srcA, Buffer<T> srcB, Buffer<T> dst);
    template<typename T> void mul(Buffer<T> srcA, Buffer<T> srcB, Buffer<T> dst);
    template<typename T> void div(Buffer<T> numer, Buffer<T> denom, Buffer<T> dst);
    template<typename T> void move(Buffer<T> a, Buffer<T> b);
    template<typename T> void diff(Buffer<T> src, Buffer<T> dst);
    template<typename T> void flip(Buffer<T> a, Buffer<T> b);
    template<typename T> void divCRev(Buffer<T> src, T k, Buffer<T> dst);
    template<typename T> void mul(Buffer<T> srcA, T k, Buffer<T> dst);
    template<typename T> void mul(T* srcA, T k, T* dst, int len);
    template<typename T> void addProd(T* srcA, T k, T* dst, int len);
    template<typename T> void interleave(Buffer<T> srcA, Buffer<T> srcB, Buffer<T> dst);
    template<typename T> void conv(Buffer<T> srcA, Buffer<T> srcB, Buffer<T> dst);
    template<typename T> void sinc(Buffer<T> kernel, Buffer<T> window, T relFreq);

    // finite impulse response filter
    template<typename T> void fir(const Buffer<T>& src, Buffer<T> dst, T relFreq, bool trim = false);

    template<typename T, typename S> void roundDown(Buffer<T> a, Buffer<S> b);
    template<typename T, typename S> void convert(Buffer<T> src, Buffer<S> dst);
};
