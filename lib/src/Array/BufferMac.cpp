#ifdef USE_ACCELERATE

#include "Buffer.h"
#include "ArrayDefs.h"

#define VIMAGE_H
#include <Accelerate/Accelerate.h>
#include <algorithm>
#include <random>

#define ERROR_COUNTER globalBufferMacSizeErrorCount

int globalBufferMacSizeErrorCount = 0;

#define declareForInts(T) \
    T(Int8u)   \
    T(Int8s)   \
    T(Int16s)  \
    T(Int32s)

#define declareForCommon(T) \
    T(Int8u)   \
    T(Int8s)   \
    T(Int16s)  \
    T(Int32s)  \
    T(Float32) \
    T(Float64)

#define constructCopyTo(T)                       \
    template<>                                   \
    void Buffer<T>::copyTo(Buffer buff) const {  \
        jassert(buff.sz >= sz);                  \
        memcpy(ptr, buff.get(), sz * sizeof(T)); \
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
        jassert(phase >= 0 && phase < sz);       \
        if (phase == 0) return *this;            \
        const int firstPartSize = sz - phase;    \
        std::memcpy(workBuffer.get(), ptr + phase, firstPartSize * sizeof(T)); \
        std::memcpy(workBuffer.get() + firstPartSize, ptr, phase * sizeof(T)); \
        std::memcpy(ptr, workBuffer.get(), sz * sizeof(T)); \
        return *this;                            \
    }

declareForInts(constructZero);
declareForInts(constructZeroSz);
declareForInts(constructSet);
declareForCommon(constructCopyTo);
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
defineVdspNullary_Real(hann, hann_window)
defineVdspNullary_Real(blackman, blkman_window)

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
    

template<> Buffer<Float32>& Buffer<Float32>::sort() { EMPTY_CHECK vDSP_vsort(ptr, 1, vDSP_Length(sz)); return *this; }
template<> Buffer<Float64>& Buffer<Float64>::sort() { EMPTY_CHECK vDSP_vsortD(ptr, 1, vDSP_Length(sz)); return *this; }

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
    for (int i = 0; i < sz; ++i) {
        sum += std::abs(ptr[i]);
    }
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
    vDSP_vsmul(VDSP_SRC1_SRC2_AUTO_PATTERN);
    return *this;
}
template<>
Buffer<Float64>& Buffer<Float64>::addProduct(Buffer src1, Buffer src2) {
    vDSP_vsmulD(VDSP_SRC1_SRC2_AUTO_PATTERN);
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

template<> Buffer<Complex32>& Buffer<Complex32>::addProduct(Buffer src1, Buffer src2) {
    CPLX_TRIADIC_SETUP(src1, src2, (*this));
    vDSP_zvma(&srcA, 2, &srcB, 2, &dest, 2, &dest, 2, vDSP_Length(sz));
    return *this;
}

#define CPLX_AUTO_FN_ARG_PATTERN &src, 2, &dest, 2, &dest, 2, vDSP_Length(sz)

#define defineComplexBuffOp(name, conjArg) \
    template<> \
    Buffer<Complex32>& Buffer<Complex32>::name(Buffer buff) { \
        BUFF_CHECK \
        CPLX_DIADIC_SETUP \
        vDSP_zv##name(CPLX_AUTO_FN_ARG_PATTERN conjArg); \
        return *this; \
    }

defineAddSubMulDiv(defineComplexBuffOp)

// c - a[i]
template<>
Buffer<Float32>& Buffer<Float32>::subCRev(Float32 c) {
    vDSP_vneg(VDSP_AUTO_ARG_PATTERN);
    vDSP_vsadd(VDSP_AUTO_ARGC_PATTERN);
    return *this;
}

template<>
Buffer<Float32>& Buffer<Float32>::subCRev(Float32 c, Buffer buff) {
    vDSP_vneg(buff.get(), 1, ptr, 1, vDSP_Length(sz));
    vDSP_vsadd(VDSP_AUTO_ARGC_PATTERN);
    return *this;
}

template<>
Buffer<Float64>& Buffer<Float64>::subCRev(Float64 c) {
    vDSP_vnegD(VDSP_AUTO_ARG_PATTERN);
    vDSP_vsaddD(VDSP_AUTO_ARGC_PATTERN);
    return *this;
}

template<>
Buffer<Float64>& Buffer<Float64>::subCRev(Float64 c, Buffer buff) {
    vDSP_vnegD(buff.get(), 1, ptr, 1, vDSP_Length(sz));
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
    Float32 mean_sq, variance;
    Float32 mean_val = mean();
    vDSP_vsq  (VDSP_AUTO_ARG_PATTERN);
    vDSP_meanv(ptr, 1, &mean_sq, vDSP_Length(sz));
    variance = mean_sq - (mean_val * mean_val);
    return std::sqrt(variance);
}

template<>
Float64 Buffer<Float64>::stddev() const {
    Float64 mean_sq, variance;
    Float64 mean_val = mean();
    vDSP_vsqD  (VDSP_AUTO_ARG_PATTERN);
    vDSP_meanvD(ptr, 1, &mean_sq, vDSP_Length(sz));
    variance = mean_sq - (mean_val * mean_val);
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

// Threshold operations
template<> Buffer<Float32>& Buffer<Float32>::threshLT(Float32 c) { vDSP_vthr(VDSP_AUTO_ARGC_PATTERN); return *this; }
template<> Buffer<Float64>& Buffer<Float64>::threshLT(Float64 c) { vDSP_vthrD(VDSP_AUTO_ARGC_PATTERN); return *this; }

template<> Buffer<Complex32>& Buffer<Complex32>::threshLT(Complex32 c) {
    // Complex32 is stored as pairs of floats [real0,imag0,real1,imag1,...]
    float* float_ptr = reinterpret_cast<float*>(ptr);
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
    vDSP_minvi(ptr, 1, &pMin, (vDSP_Length*)&index, vDSP_Length(sz));
}

template<>
void Buffer<Float32>::getMax(Float32& pMax, int& index) const {
    vDSP_maxvi(ptr, 1, &pMax, (vDSP_Length*)&index, vDSP_Length(sz));
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
    if (factor < 0)
        factor = buff.size() / sz;
    if (sz == 0)
        return 0;
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
    if (factor < 0)
        factor = buff.size() / sz;
    if (sz == 0)
        return 0;
    if (factor == 1) {
        buff.copyTo(*this);
        return 0;
    }

    const float offset = 0;
    const int srcLen = buff.size();
    const int dstLen = srcLen / factor;

    // Extract samples at the specified phase
    // This is equivalent to taking every nth sample where n = factor
    vDSP_vsadd(buff.get() + phase,  // Source buffer starting at phase
               factor,              // Source stride = factor
               &offset,             // Add zero (no offset needed)
               ptr,                 // Destination buffer
               1,                   // Destination stride
               dstLen);             // Number of elements to process

    return phase;
}

template<> Buffer<Float32>& Buffer<Float32>::undenormalize() { return *this; }
template<> Buffer<Float64>& Buffer<Float64>::undenormalize() { return *this; }


#endif // USE_ACCELERATE