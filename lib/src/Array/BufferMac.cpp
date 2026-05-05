#include "ScopedAlloc.h"
#ifdef USE_ACCELERATE

#include "Buffer.h"
#include "ArrayDefs.h"

#define VIMAGE_H
#include <Accelerate/Accelerate.h>
#include <algorithm>
#include <cmath>
#include <random>

#define ERROR_COUNTER globalBufferMacSizeErrorCount

int globalBufferMacSizeErrorCount = 0;

#define declareForIntsAndCplx(T) \
    T(Int8u)   \
    T(Int8s)   \
    T(Int16s)  \
    T(Int32s)  \
    T(Complex32)

#define declareForComplex(T) \
    T(Complex32)

#define declareForCommon(T) \
    T(Int8u)   \
    T(Int8s)   \
    T(Int16s)  \
    T(Int32s)  \
    T(Float32) \
    T(Float64)

#define declareForAll(T) \
    T(Int8u)   \
    T(Int8s)   \
    T(Int16s)  \
    T(Int32s)  \
    T(Float32) \
    T(Float64) \
    T(Complex32)


#define constructCopyTo(T)                       \
    template<>                                   \
    void Buffer<T>::copyTo(Buffer buff) const {  \
        memcpy(buff.get(), ptr, jmin(sz, buff.sz) * sizeof(T)); \
    }

#define constructZero(T)                         \
    template<>                                   \
    Buffer<T>& Buffer<T>::zero() {               \
        std::memset(ptr, 0, sz * sizeof(T));     \
        return *this;                            \
    }

#define constructZeroSz(T)                       \
    template<>                                   \
    Buffer<T>& Buffer<T>::zero(int size) {       \
        std::memset(ptr, 0, size * sizeof(T));   \
        return *this;                            \
    }

#define constructSet(T)                          \
    template<>                                   \
    Buffer<T>& Buffer<T>::set(T c) {             \
        std::fill_n(ptr, sz, c);                 \
        return *this;                            \
    }

#define constructWithPhase(T)                    \
    template<>                                   \
    Buffer<T>& Buffer<T>::withPhase(int phase, Buffer workBuffer) { \
        jassert(phase >= 0);       \
        if(phase == 0 || sz == 0)                \
            return *this;                        \
        phase = phase % sz;                      \
        section(phase, sz - phase).copyTo(workBuffer); \
        copyTo(workBuffer.section(sz - phase, phase)); \
        workBuffer.copyTo(*this);                \
        return *this;                            \
    }

declareForIntsAndCplx(constructZero);
declareForIntsAndCplx(constructZeroSz);
declareForIntsAndCplx(constructSet);
declareForAll(constructCopyTo);
declareForCommon(constructWithPhase);

#define VFORCE_AUTO_ARG_PATTERN  ptr, ptr, &sz
#define VDSP_AUTO_ARG_PATTERN    ptr, 1, ptr, 1, vDSP_Length(sz)
#define VDSP_AUTO_ARGC_PATTERN   ptr, 1, &c, ptr, 1, vDSP_Length(sz)
#define VDSP_BUFF_ARGC_PATTERN   ptr, 1, buff.get(), 1, &result, vDSP_Length(sz)
#define VDSP_SRC1_SRC2_AUTO_PATTERN   src1.get(), 1, src2.get(), ptr, 1, vDSP_Length(sz)
#define VDSP_NULLARY_ARG_PATTERN ptr, 1, vDSP_Length(sz)

#define defineFn(name, type, fn, args_pattern) \
   template<> Buffer<type>& Buffer<type>::name() { \
      EMPTY_CHECK \
      fn(args_pattern); \
      return *this;    \
   }

#define defineVforceAuto_Real(name, fn) \
  defineFn(name, Float32, fn##f, VFORCE_AUTO_ARG_PATTERN) \
  defineFn(name, Float64, fn, VFORCE_AUTO_ARG_PATTERN)

#define defineVdspNullary_Real(name, fn) \
    defineFn(name, Float32, vDSP_##fn, VDSP_NULLARY_ARG_PATTERN) \
    defineFn(name, Float64, vDSP_##fn##D, VDSP_NULLARY_ARG_PATTERN)

#define defineVdspAuto_Real(name, fn) \
    defineFn(name, Float32, vDSP_##fn, VDSP_AUTO_ARG_PATTERN) \
    defineFn(name, Float64, vDSP_##fn##D, VDSP_AUTO_ARG_PATTERN)

defineVdspNullary_Real(zero, vclr)
defineVdspNullary_Real(flip, vrvrs)
defineVforceAuto_Real (sqrt, vvsqrt)
defineVforceAuto_Real (exp,  vvexp)
defineVforceAuto_Real (tanh, vvtanh)
defineVforceAuto_Real (sin,  vvsin)
defineVforceAuto_Real (ln,   vvlog)
defineVforceAuto_Real (inv,  vvrec)
defineVdspAuto_Real   (abs,  vabs)
defineVdspAuto_Real   (sqr,  vsq)

template<> Buffer<Float32>& Buffer<Float32>::zero(int size) { SIZE_CHECK vDSP_vclr (ptr, 1, vDSP_Length(size)); return *this; }
template<> Buffer<Float64>& Buffer<Float64>::zero(int size) { SIZE_CHECK vDSP_vclrD(ptr, 1, vDSP_Length(size)); return *this; }

template<> Buffer<Float32>& Buffer<Float32>::set(Float32 c) { EMPTY_CHECK    vDSP_vfill  (&c, ptr, 1, vDSP_Length(sz)); return *this; }
template<> Buffer<Float64>& Buffer<Float64>::set(Float64 c) { EMPTY_CHECK    vDSP_vfillD (&c, ptr, 1, vDSP_Length(sz)); return *this; }
template<> Buffer<Float32>& Buffer<Float32>::add(Float32 c) { NULL_ARG_CHECK vDSP_vsadd  (VDSP_AUTO_ARGC_PATTERN); return *this; }
template<> Buffer<Float64>& Buffer<Float64>::add(Float64 c) { NULL_ARG_CHECK vDSP_vsaddD (VDSP_AUTO_ARGC_PATTERN); return *this; }
template<> Buffer<Float32>& Buffer<Float32>::mul(Float32 c) { UNO_ARG_CHECK  vDSP_vsmul  (VDSP_AUTO_ARGC_PATTERN); return *this; }
template<> Buffer<Float64>& Buffer<Float64>::mul(Float64 c) { UNO_ARG_CHECK  vDSP_vsmulD (VDSP_AUTO_ARGC_PATTERN); return *this; }
template<> Buffer<Int16s>& Buffer<Int16s>::mul(Int16s c) {
    if (ptr == nullptr || c == 1) {
        return *this;
    }

    for (int i = 0; i < sz; ++i) {
        ptr[i] = (Int16s) ((int) ptr[i] * (int) c);
    }

    return *this;
}
template<> Buffer<Float32>& Buffer<Float32>::sub(Float32 c) { return add(-c); }
template<> Buffer<Float64>& Buffer<Float64>::sub(Float64 c) { return add(-c); }
template<> Buffer<Float32>& Buffer<Float32>::div(Float32 c) { NULL_ARG_CHECK return mul(1.0f / c); }
template<> Buffer<Float64>& Buffer<Float64>::div(Float64 c) { NULL_ARG_CHECK return mul(1.0 / c);  }
template<> Buffer<Float32>& Buffer<Float32>::pow(Float32 c) { UNO_ARG_CHECK  vvpowsf(ptr, &c, ptr, &sz); return *this; }
template<> Buffer<Float64>& Buffer<Float64>::pow(Float64 c) { UNO_ARG_CHECK  vvpows(ptr, &c, ptr, &sz); return *this; }

#define VDSP_UNARY_BUFF_PATTERN  buff.get(), 1, ptr, 1, ptr, 1, vDSP_Length(sz)

#define defineUnaryBuffFn(name, type, fn, args_pattern) \
    template<> Buffer<type>& Buffer<type>::name(Buffer<type> buff) { \
    if(buff.size() < sz) { ++ERROR_COUNTER; return *this; } \
    fn(args_pattern);     \
    return *this;         \
}

#define defineVdspUnaryBuff_Real(name) \
    defineUnaryBuffFn(name, Float32, vDSP_v##name,    VDSP_UNARY_BUFF_PATTERN) \
    defineUnaryBuffFn(name, Float64, vDSP_v##name##D, VDSP_UNARY_BUFF_PATTERN)

defineVdspUnaryBuff_Real(add)
defineVdspUnaryBuff_Real(sub)
defineVdspUnaryBuff_Real(mul)
defineVdspUnaryBuff_Real(div)

    
#define defineNullaryConstFn(name, type, fn) \
    template<> type Buffer<type>::name() const { \
    if(sz == 0) return 0; \
    type result; \
    vDSP_##fn(ptr, 1, &result, vDSP_Length(sz)); \
    return result; \
}

#define defineVdspNullaryConst_Real(name, fn) \
    defineNullaryConstFn(name, Float32, fn) \
    defineNullaryConstFn(name, Float64, fn##D)
  
defineVdspNullaryConst_Real(sum,  sve)
defineVdspNullaryConst_Real(mean, meanv)
defineVdspNullaryConst_Real(min,  minv)
defineVdspNullaryConst_Real(max,  maxv)

template<> Buffer<Float32>& Buffer<Float32>::sort() { EMPTY_CHECK vDSP_vsort(ptr, vDSP_Length(sz), 1); return *this; }
template<> Buffer<Float64>& Buffer<Float64>::sort() { EMPTY_CHECK vDSP_vsortD(ptr, vDSP_Length(sz), 1); return *this; }


template<>
Buffer<Float32> &Buffer<Float32>::hann() {
    if (sz == 0) return *this;
    if (sz == 1) {
        ptr[0] = 1.f;
        return *this;
    }

    constexpr Float32 start = 0.f;
    const Float32 step = Float32(2.0 * M_PI / double(sz - 1));
    const Float32 scale = -0.5f;
    const Float32 offset = 0.5f;
    int size = sz;

    vDSP_vramp(&start, &step, ptr, 1, vDSP_Length(sz));
    vvcosf(ptr, ptr, &size);
    vDSP_vsmsa(ptr, 1, &scale, &offset, ptr, 1, vDSP_Length(sz));
    return *this;
}

template<>
Buffer<Float64> &Buffer<Float64>::hann() {
    if (sz == 0) return *this;
    if (sz == 1) {
        ptr[0] = 1.0;
        return *this;
    }

    constexpr Float64 start = 0.0;
    const Float64 step = 2.0 * M_PI / double(sz - 1);
    const Float64 scale = -0.5;
    const Float64 offset = 0.5;
    int size = sz;

    vDSP_vrampD(&start, &step, ptr, 1, vDSP_Length(sz));
    vvcos(ptr, ptr, &size);
    vDSP_vsmsaD(ptr, 1, &scale, &offset, ptr, 1, vDSP_Length(sz));
    return *this;
}

template<>
Buffer<Float32> &Buffer<Float32>::blackman() {
    if (sz == 0) return *this;
    if (sz == 1) {
        ptr[0] = 1.f;
        return *this;
    }

    ScopedAlloc<Float32> cos2Mem(sz);
    Buffer<Float32> cos2(cos2Mem, sz);
    constexpr Float32 start = 0.f;
    constexpr Float32 two = 2.f;
    const Float32 step = Float32(2.0 * M_PI / double(sz - 1));
    const Float32 a0 = 0.42f;
    const Float32 a1 = -0.5f;
    const Float32 a2 = 0.08f;
    int size = sz;

    vDSP_vramp(&start, &step, ptr, 1, vDSP_Length(sz));
    vDSP_vsmul(ptr, 1, &two, cos2.get(), 1, vDSP_Length(sz));
    vvcosf(ptr, ptr, &size);
    vvcosf(cos2.get(), cos2.get(), &size);
    vDSP_vsmsa(ptr, 1, &a1, &a0, ptr, 1, vDSP_Length(sz));
    vDSP_vsma(cos2.get(), 1, &a2, ptr, 1, ptr, 1, vDSP_Length(sz));
    return *this;
}

template<>
Buffer<Float64> &Buffer<Float64>::blackman() {
    if (sz == 0) return *this;
    if (sz == 1) {
        ptr[0] = 1.0;
        return *this;
    }

    ScopedAlloc<Float64> cos2Mem(sz);
    Buffer<Float64> cos2(cos2Mem, sz);
    constexpr Float64 start = 0.0;
    constexpr Float64 two = 2.0;
    const Float64 step = 2.0 * M_PI / double(sz - 1);
    const Float64 a0 = 0.42;
    const Float64 a1 = -0.5;
    const Float64 a2 = 0.08;
    int size = sz;

    vDSP_vrampD(&start, &step, ptr, 1, vDSP_Length(sz));
    vDSP_vsmulD(ptr, 1, &two, cos2.get(), 1, vDSP_Length(sz));
    vvcos(ptr, ptr, &size);
    vvcos(cos2.get(), cos2.get(), &size);
    vDSP_vsmsaD(ptr, 1, &a1, &a0, ptr, 1, vDSP_Length(sz));
    vDSP_vsmaD(cos2.get(), 1, &a2, ptr, 1, ptr, 1, vDSP_Length(sz));
    return *this;
}

template<>
void Buffer<Float32>::minmax(Float32& pMin, Float32& pMax) const {
    if(sz == 0) {
        pMin = 0;
        pMax = 0;
    }
    vDSP_minv(ptr, 1, &pMin, vDSP_Length(sz));
    vDSP_maxv(ptr, 1, &pMax, vDSP_Length(sz));
}

template<>
Float32 Buffer<Float32>::dot(Buffer buff) const {
    Float32 result;
    vDSP_dotpr(VDSP_BUFF_ARGC_PATTERN);
    return result;
}

template<>
Float64 Buffer<Float64>::dot(Buffer buff) const {
    Float64 result;
    vDSP_dotprD(VDSP_BUFF_ARGC_PATTERN);
    return result;
}

template<>
Float32 Buffer<Float32>::normL1() const {
    Float32 sum = 0;
    vDSP_svemg(ptr, 1, &sum, vDSP_Length(sz));
    return sum;
}

#define defineNormL2(type, name, fn) \
template<> \
type Buffer<type>::name() const { \
    EMPTY_CHECK_ZERO \
    type result; \
    vDSP_##fn(ptr, 1, ptr, 1, &result, vDSP_Length(sz)); \
    return std::sqrt(result); \
}

defineNormL2(Float32, normL2, dotpr)
defineNormL2(Float64, normL2, dotprD)

#define defineNormDiffL2(type, name, fn) \
  template<> type Buffer<type>::name(Buffer<type> buff) const { \
    if(buff.size() == 0) return 0; \
    type result; \
    vDSP_##fn(VDSP_BUFF_ARGC_PATTERN); \
    return std::sqrt(result);  \
}

defineNormDiffL2(Float32, normDiffL2, distancesq)
defineNormDiffL2(Float64, normDiffL2, distancesqD)


// Add product

template<>
Buffer<Float32>& Buffer<Float32>::addProduct(Buffer src1, Buffer src2) {
    if (sz == 0) return *this;
    vDSP_vma(src1.get(), 1, src2.get(), 1, ptr, 1, ptr, 1, vDSP_Length(sz));
    return *this;
}
template<>
Buffer<Float64>& Buffer<Float64>::addProduct(Buffer src1, Buffer src2) {
    if (sz == 0) return *this;
    vDSP_vmaD(src1.get(), 1, src2.get(), 1, ptr, 1, ptr, 1, vDSP_Length(sz));
    return *this;
}

template<> Buffer<Complex32>& Buffer<Complex32>::addProduct(Buffer src1, Buffer src2) {
    int size = jmin(sz, src1.size(), src2.size());
    CPLX_TRIADIC_SETUP(src1, src2, (*this));

    // dest uses interleaved real/imag pointers with stride 2, so vDSP writes
    // directly into ptr as std::complex<float> storage. Do not repack after.
    vDSP_zvma(&srcA, 2, &srcB, 2, &dest, 2, &dest, 2, vDSP_Length(size));
    return *this;
}

template<>
Buffer<Float32>& Buffer<Float32>::addProduct(Buffer src, Float32 k) {
    vDSP_vsma(src.get(), 1, &k, ptr, 1, ptr, 1, vDSP_Length(sz));
    return *this;
}
template<>
Buffer<Float64>& Buffer<Float64>::addProduct(Buffer src, Float64 k) {
    vDSP_vsmaD(src.get(), 1, &k, ptr, 1, ptr, 1, vDSP_Length(sz));
    return *this;
}

template<> Buffer<Complex32>& Buffer<Complex32>::addProduct(Buffer src, Complex32 k) {
    int size = jmin(sz, src.size());
    ScopedAlloc<Complex32> temp(size);
    temp.set(k);
    return addProduct(src.withSize(size), temp);
}

//
#define CPLX_AUTO_FN_ARG_PATTERN &src, 2, &dest, 2, &dest, 2, vDSP_Length(sz)

//
#define CPLX_AUTO_FN_ARG_PATTERN2 &dest, 2, &src, 2, &dest, 2, vDSP_Length(sz)

#define defineComplexBuffOp(name, conjArg) \
    template<> \
    Buffer<Complex32>& Buffer<Complex32>::name(Buffer buff) { \
        BUFF_CHECK \
        CPLX_DIADIC_SETUP \
        vDSP_zv##name(&dest, 2, &src, 2, &dest, 2, vDSP_Length(sz) conjArg); \
        return *this; \
    }

#define defineComplexDivBuff \
    template<> \
    Buffer<Complex32>& Buffer<Complex32>::div(Buffer buff) { \
        BUFF_CHECK \
        CPLX_DIADIC_SETUP \
        vDSP_zvdiv(&src, 2, &dest, 2, &dest, 2, vDSP_Length(sz)); \
        return *this; \
    }

#define defineComplexConstOp(name, conjArg)                   \
    template<>                                                \
    Buffer<Complex32>& Buffer<Complex32>::name(Complex32 c) { \
        if(sz == 0) return *this;                             \
        ScopedAlloc<Complex32> temp(sz);                      \
        temp.set(c);                                          \
        DSPSplitComplex dest, src;                            \
        dest.realp = reinterpret_cast<float*>(ptr);           \
        dest.imagp = dest.realp + 1;                          \
        src.realp = reinterpret_cast<float*>(temp.get());     \
        src.imagp = src.realp + 1;                            \
        vDSP_zv##name(&dest, 2, &src, 2, &dest, 2, vDSP_Length(sz) conjArg); \
        return *this;                                         \
    }

#define defineComplexConstDiv                                 \
    template<>                                                \
    Buffer<Complex32>& Buffer<Complex32>::div(Complex32 c) {  \
        if(sz == 0) return *this;                             \
        ScopedAlloc<Complex32> temp(sz);                      \
        temp.set(c);                                          \
        DSPSplitComplex dest, src;                            \
        dest.realp = reinterpret_cast<float*>(ptr);           \
        dest.imagp = dest.realp + 1;                          \
        src.realp = reinterpret_cast<float*>(temp.get());     \
        src.imagp = src.realp + 1;                            \
        vDSP_zvdiv(&src, 2, &dest, 2, &dest, 2, vDSP_Length(sz)); \
        return *this;                                         \
    }

// ptr[i] = ptr[i] op C
defineAddSubMulDiv(defineComplexConstOp)
defineComplexConstDiv
// ptr[i] = ptr[i] op buff[i]
defineAddSubMulDiv(defineComplexBuffOp)
defineComplexDivBuff

// c - a[i]
template<>
Buffer<Float32>& Buffer<Float32>::subCRev(Float32 c) {
    vDSP_vneg(VDSP_AUTO_ARG_PATTERN);
    vDSP_vsadd(VDSP_AUTO_ARGC_PATTERN);
    return *this;
}

template<>
Buffer<Float64>& Buffer<Float64>::subCRev(Float64 c) {
    vDSP_vnegD(VDSP_AUTO_ARG_PATTERN);
    vDSP_vsaddD(VDSP_AUTO_ARGC_PATTERN);
    return *this;
}

// c / a[i]
template<>
Buffer<Float32>& Buffer<Float32>::divCRev(Float32 c) {
    EMPTY_CHECK
    if(c == 0.f) return zero();
    vvrecf(VFORCE_AUTO_ARG_PATTERN);
    if(c == 1.f) return *this;
    vDSP_vsmul(VDSP_AUTO_ARGC_PATTERN);
    return *this;
}

template<>
Buffer<Float64>& Buffer<Float64>::divCRev(Float64 c) {
    EMPTY_CHECK
    if(c == 0) return zero();
    vvrec(VFORCE_AUTO_ARG_PATTERN);
    if(c == 1.f) return *this;
    vDSP_vsmulD(VDSP_AUTO_ARGC_PATTERN);
    return *this;
}

template<>
Buffer<Float32>& Buffer<Float32>::powCRev(Float32 k) {
    EMPTY_CHECK
    if(k == 1.f) return *this;
    float c = std::log(k);
    vDSP_vsmul(VDSP_AUTO_ARGC_PATTERN);
    vvexpf(VFORCE_AUTO_ARG_PATTERN);
    return *this;
}
template<>
Buffer<Float64>& Buffer<Float64>::powCRev(Float64 k) {
    EMPTY_CHECK
    if(k == 1.) return *this;
    Float64 c = std::log(k);
    vDSP_vsmulD(VDSP_AUTO_ARGC_PATTERN);
    vvexp(VFORCE_AUTO_ARG_PATTERN);
    return *this;
}

// statistics

template<>
Float32 Buffer<Float32>::stddev() const {
    EMPTY_CHECK_ZERO
    Float32 variance = 0;
    Float32 meanVal = mean();
    Float32 negMean = -meanVal;
    ScopedAlloc<Float32> diffMem(sz);
    Buffer<Float32> diff(diffMem, sz);

    vDSP_vsadd(ptr, 1, &negMean, diff.get(), 1, vDSP_Length(sz));
    vDSP_measqv(diff.get(), 1, &variance, vDSP_Length(sz));
    variance *= Float32(sz) / Float32(sz - 1);
    return std::sqrt(variance);
}

template<>
Float64 Buffer<Float64>::stddev() const {
    EMPTY_CHECK_ZERO
    Float64 variance = 0;
    Float64 meanVal = mean();
    Float64 negMean = -meanVal;
    ScopedAlloc<Float64> diffMem(sz);
    Buffer<Float64> diff(diffMem, sz);

    vDSP_vsaddD(ptr, 1, &negMean, diff.get(), 1, vDSP_Length(sz));
    vDSP_measqvD(diff.get(), 1, &variance, vDSP_Length(sz));
    variance *= Float64(sz) / Float64(sz - 1);
    return std::sqrt(variance);
}

// vector ramp
template<>
Buffer<Float32>& Buffer<Float32>::ramp(Float32 offset, Float32 delta) {
    vDSP_vramp(&offset, &delta, ptr, 1, vDSP_Length(sz));
    return *this;
}
template<>
Buffer<Float64>& Buffer<Float64>::ramp(Float64 offset, Float64 delta) {
    vDSP_vrampD(&offset, &delta, ptr, 1, vDSP_Length(sz));
    return *this;
}

template<> Buffer<Float32>& Buffer<Float32>::ramp() { if(sz > 1) { return ramp(0.f, 1.f / (sz - 1)); } return *this; }
template<> Buffer<Float64>& Buffer<Float64>::ramp() { if(sz > 1) { return ramp(0., 1. / (sz - 1)); } return *this; }


#define constructSinInit(T) \
template<> \
    Buffer<T>& Buffer<T>::sin(float relFreq, float unitPhase) { \
        if(sz == 0) return *this; \
        return ramp( \
            static_cast<T>(unitPhase * 2 * M_PI), \
            static_cast<T>(relFreq * 2 * M_PI) \
        ).sin(); \
    }

constructSinInit(Float32);
constructSinInit(Float64);

// Threshold operations
template<> Buffer<Float32>& Buffer<Float32>::threshLT(Float32 c) { vDSP_vthr(VDSP_AUTO_ARGC_PATTERN); return *this; }
template<> Buffer<Float64>& Buffer<Float64>::threshLT(Float64 c) { vDSP_vthrD(VDSP_AUTO_ARGC_PATTERN); return *this; }

template<> Buffer<Complex32>& Buffer<Complex32>::threshLT(Complex32 c) {
    // Complex32 is stored as pairs of floats [real0,imag0,real1,imag1,...]
    auto* float_ptr = reinterpret_cast<float*>(ptr);
    float thresh_real = c.real();
    float thresh_imag = c.imag();
    jassert(thresh_real == thresh_imag);
    vDSP_vthr(float_ptr, 1, &thresh_real, float_ptr, 1, vDSP_Length(sz * 2));
    return *this;
}

template<> Buffer<Float32>& Buffer<Float32>::threshGT(Float32 c) {
    Float32 lowThresh = -INFINITY;
    vDSP_vclip(ptr, 1, &lowThresh, &c, ptr, 1, vDSP_Length(sz));
    return *this;
}
template<> Buffer<Float64>& Buffer<Float64>::threshGT(Float64 c) {
    Float64 lowThresh = -INFINITY;
    vDSP_vclipD(ptr, 1, &lowThresh, &c, ptr, 1, vDSP_Length(sz));
    return *this;
}

template<> Buffer<Float32>& Buffer<Float32>::clip(Float32 low, Float32 high) {
    vDSP_vclip(ptr, 1, &low, &high, ptr, 1, vDSP_Length(sz));
    return *this;
}
template<> Buffer<Float64>& Buffer<Float64>::clip(Float64 low, Float64 high) {
    vDSP_vclipD(ptr, 1, &low, &high, ptr, 1, vDSP_Length(sz));
    return *this;
}

template<>
void Buffer<Float32>::getMin(Float32& pMin, int& index) const {
    long minIdx;
    vDSP_minvi(ptr, 1, &pMin, (vDSP_Length*)&minIdx, vDSP_Length(sz));
    index = minIdx;
}

template<>
void Buffer<Float32>::getMax(Float32& pMax, int& index) const {
    long maxIdx;
    vDSP_maxvi(ptr, 1, &pMax, (vDSP_Length*)&maxIdx, vDSP_Length(sz));
    index = maxIdx;
}

template<>
Buffer<Float32>& Buffer<Float32>::rand(unsigned& seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<Float32> dist(0, 1);
    for (size_t i = 0; i < sz; i++) {
        ptr[i] = dist(gen);
    }
    seed = gen();
    return *this;
}

template<>
bool Buffer<Float32>::isProbablyEmpty() const {
    if (sz == 0) return true;
    Float32 result;
    vDSP_sve(ptr, 1, &result, vDSP_Length(sz));
    return result == 0.0f;
}

template <>
int Buffer<Float32>::upsampleFrom(Buffer<Float32> buff, int factor, int phase) {
    if (sz == 0 || buff.empty())
        return 0;
    if (factor < 0)
        factor = sz / buff.size();
    if (factor == 1) {
        buff.copyTo(*this);
        return 0;
    }

    // Calculate destination size
    const float offset = 0;
    const int srcLen = buff.size();
    const int dstLen = srcLen * factor;
    if(dstLen > sz) {
        ++ERROR_COUNTER;
        return 0;
    }

    vDSP_vclr(ptr, 1, dstLen);

    // Place the source samples at the correct phase positions
    // This is equivalent to the IPP behavior where:
    // pDst[factor * n + phase] = pSrc[n]
    vDSP_vsadd(buff.get(),            // Source buffer
               1,                     // Source stride
               &offset,               // Add zero (no offset needed)
               ptr + phase,           // Destination with phase offset
               vDSP_Stride(factor),   // Destination stride = factor
               vDSP_Length(srcLen));  // Number of elements to process

    return phase;
}

template <>
int Buffer<Float32>::downsampleFrom(Buffer<Float32> buff, int factor, int phase) {
    if (sz == 0 || buff.empty())
        return 0;
    if (factor < 0)
        factor = buff.size() / sz;
    if (factor == 1) {
        buff.copyTo(*this);
        return 0;
    }

    const float offset = 0;
    const int srcLen = buff.size();
    phase = phase % factor;

    if (phase < 0) {
        phase += factor;
    }

    const int dstLen = phase < srcLen ? jmin(sz, (srcLen + factor - 1 - phase) / factor) : 0;

    // Extract samples at the specified phase
    // This is equivalent to taking every nth sample where n = factor
    if (dstLen > 0) {
        vDSP_vsadd(buff.get() + phase,  // Source buffer starting at phase
                   factor,              // Source stride = factor
                   &offset,             // Add zero (no offset needed)
                   ptr,                 // Destination buffer
                   1,                   // Destination stride
                   dstLen);             // Number of elements to process
    }

    return (factor + phase - srcLen % factor) % factor;
}

#define implementOperators(T)                                  \
template<>                                                     \
void Buffer<T>::operator+=(const Buffer<T>& other) { \
    add(other);                                                \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<T>::operator+=(T val) {                  \
    add(val);                                                  \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<T>::operator-=(const Buffer<T>& other) { \
    sub(other);                                                \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<T>::operator-=(T val) {                  \
    sub(val);                                                  \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<T>::operator*=(const Buffer<T>& other) { \
    mul(other);                                                \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<T>::operator*=(T val) {                  \
    mul(val);                                                  \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<T>::operator/=(const Buffer<T>& other) { \
    div(other);                                                \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<T>::operator/=(T val) {                  \
    div(val);                                                  \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<T>::operator<<(const Buffer<T>& other) { \
    other.copyTo(*this);                                       \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<T>::operator>>(Buffer<T> other) const {  \
    copyTo(other);                                             \
}

implementOperators(Float32)
implementOperators(Float64)

defineAudioBufferConstructor(Float32)
defineAudioBufferConstructor(Float64)

#endif // USE_ACCELERATE
