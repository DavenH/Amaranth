#include "Buffer.h"
#include "ScopedAlloc.h"

#include "ipp.h"

#pragma warning(disable: 4661)

#define declareForCommonTypes(T) \
    T(8u)    \
    T(16s)   \
    T(32s)   \
    T(32f)   \
    T(64f)   \
    T(32fc)

#define declareForReal(T) \
    T(32f)    \
    T(64f)

#define declareForRealAndCplx(T) \
    T(32f)    \
    T(64f)    \
    T(32fc)

#define declareForFloatAndCplx(T) \
    T(32f)    \
    T(32fc)

template class Buffer<Ipp8u>;
template class Buffer<Ipp8s>;
template class Buffer<Ipp16s>;
template class Buffer<Ipp32s>;
template class Buffer<Ipp32f>;
template class Buffer<Ipp64f>;
template class Buffer<Ipp32fc>;

template<>
bool Buffer<float>::isProbablyEmpty() const {
    if(sz == 0)
        return true;

    bool isEmpty = ptr[sz - 1] == 0;
    int size      = sz / 2;

    while (isEmpty && size > 0) {
        isEmpty &= ptr[size] == 0;
        size >>= 1;
    }

    return isEmpty;
}

template<>
Buffer<double>::Buffer(AudioSampleBuffer& audioBuffer, int chan) : ptr(nullptr), sz(0) {
}

template<>
Buffer<float>::Buffer(AudioSampleBuffer& audioBuffer, int chan) :
        ptr(audioBuffer.getWritePointer(chan)),
        sz(audioBuffer.getNumSamples()) {
}

#define constructStddev(T)                                      \
template<>                                                      \
Ipp##T Buffer<Ipp##T>::stddev()    const                        \
{                                                               \
    Ipp##T deviation;                                           \
    ippsStdDev_##T(ptr, sz, &deviation, ippAlgHintFast);        \
    return deviation;                                           \
}

#define constructSort(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::sort()                          \
{                                                               \
    ippsSortAscend_##T##_I(ptr, sz);                            \
    return *this;                                               \
}

#define constructZero(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::zero()                          \
{                                                               \
    ippsZero_##T(ptr, sz);                                      \
    return *this;                                               \
}

#define constructZeroSz(T)                                      \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::zero(int size)                  \
{                                                               \
    if(sz == 0)                                                 \
        return *this;                                           \
    jassert(size <= sz);                                        \
    ippsZero_##T(ptr, size);                                    \
    return *this;                                               \
}

#define constructSet(T)                                         \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::set(Ipp##T value)               \
{                                                               \
    ippsSet_##T(value, ptr, sz);                                \
    return *this;                                               \
}

#define constructAdd(T)                                         \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::add(Buffer<Ipp##T> buff)        \
{                                                               \
    ippsAdd_##T##_I(buff.ptr, ptr, jmin(sz, buff.sz));          \
    return *this;                                               \
}

#define constructAdd2(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::add(Buffer<Ipp##T> src1, Buffer<Ipp##T> src2) \
{                                                               \
    ippsAdd_##T(src1.ptr, src2.ptr, ptr, jmin(sz, src1.sz, src2.sz)); \
    return *this;                                               \
}

#define constructAddC(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::add(Ipp##T c)                   \
{                                                               \
    if(c != Ipp##T())                                           \
        ippsAddC_##T##_I(c, ptr, sz);                           \
    return *this;                                               \
}

#define constructSubC(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::sub(Ipp##T c)                   \
{                                                               \
    if(c != Ipp##T())                                           \
        ippsSubC_##T##_I(c, ptr, sz);                           \
    return *this;                                               \
}

#define constructAddBuffC(T)                                    \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::add(Buffer<Ipp##T> buff, Ipp##T c) \
{                                                               \
    if(c == Ipp##T())                                           \
        ippsAdd_##T##_I(buff.ptr, ptr, jmin(buff.sz,sz));       \
    else                                                        \
        ippsAddC_##T(buff.ptr, c, ptr, jmin(buff.sz,sz));       \
    return *this;                                               \
}

#define constructSub(T)                                         \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::sub(Buffer<Ipp##T> buff)        \
{                                                               \
    ippsSub_##T##_I(buff.ptr, ptr, jmin(sz, buff.sz));          \
    return *this;                                               \
}

#define constructDiff(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::diff(Buffer<Ipp##T> buff)       \
{                                                               \
    if(sz < 2)                                                  \
        return *this;                                           \
    ippsSub_##T(buff.ptr, buff.ptr + 1, ptr, jmin(buff.sz - 1, sz - 1)); \
    ptr[sz - 1] = ptr[sz - 2];                                  \
    return *this;                                               \
}

#define constructSubB(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::sub(Buffer<Ipp##T> src1, Buffer<Ipp##T> src2) \
{                                                               \
    ippsSub_##T(src1.ptr, src2.ptr, ptr, jmin(sz, src1.sz, src2.sz)); \
    return *this;                                               \
}

#define constructSubCRev(T)                                     \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::subCRev(Ipp##T c)               \
{                                                               \
    ippsSubCRev_##T##_I(c, ptr, sz);                            \
    return *this;                                               \
}

#define constructSubCRevB(T)                                    \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::subCRev(Ipp##T c, Buffer<Ipp##T> buff) \
{                                                               \
    ippsSubCRev_##T(buff.ptr, c, ptr, jmin(sz, buff.sz));       \
    return *this;                                               \
}

#define constructDivCRev(T)                                     \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::divCRev(Ipp##T c)               \
{                                                               \
    ippsDivCRev_##T##_I(c, ptr, sz);                            \
    return *this;                                               \
}

#define constructMul(T)                                         \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::mul(Buffer<Ipp##T> buff)        \
{                                                               \
    ippsMul_##T##_I(buff.ptr, ptr, jmin(sz, buff.sz));          \
    return *this;                                               \
}

#define constructDiv(T)                                         \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::div(Buffer<Ipp##T> buff)        \
{                                                               \
    ippsDiv_##T##_I(buff.ptr, ptr, jmin(sz, buff.sz));          \
    return *this;                                               \
}

#define constructMulC(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::mul(Ipp##T c)                   \
{                                                               \
    if(c != 1)                                                  \
        ippsMulC_##T##_I(c, ptr, sz);                           \
    return *this;                                               \
}

#define constructTanh(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::tanh()                          \
{                                                               \
    ippsTanh_##T##_A24(ptr, ptr, sz);                           \
    return *this;                                               \
}

#define constructDivC(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::div(Ipp##T c)                   \
{                                                               \
    if(c != 0)                                                  \
        ippsDivC_##T##_I(c, ptr, sz);                           \
    return *this;                                               \
}

template<> Buffer<Ipp32fc>& Buffer<Ipp32fc>::mul(Ipp32fc c)
{
    if(c.re != 1 || c.im != 0)
        ippsMulC_32fc_I(c, ptr, sz);
    return *this;
}

#define constructMulComb(T)                                     \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::mul(Buffer<Ipp##T> buff, Ipp##T c) \
{                                                               \
    if(c == Ipp##T(1)) {                                        \
        buff.copyTo(*this);                                     \
    } else {                                                    \
        ippsMulC_##T(buff.ptr, c, ptr, jmin(sz, buff.sz));      \
    }                                                           \
    return *this;                                               \
}

#define constructMulComb2(T)                                    \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::mul(Buffer<Ipp##T> src1, Buffer<Ipp##T> src2) \
{                                                               \
    ippsMul_##T(src1.ptr, src2.ptr, ptr, jmin(sz, src1.sz, src2.sz)); \
    return *this;                                               \
}


#define constructConv(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::conv(Buffer<Ipp##T> src1, Buffer<Ipp##T> src2, Buffer<Ipp8u> workBuff) \
{                                                               \
    jassert(sz >= src1.size() + src2.size() - 1);               \
    ippsConvolve_##T(src1, src1.sz, src2, src2.sz, ptr, IppAlgType::ippAlgAuto, workBuff); \
    return *this;                                               \
}


#define constructSqrt(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::sqrt()                          \
{                                                               \
    ippsSqrt_##T##_A11(ptr, ptr, sz);                           \
    return *this;                                               \
}


#define constructAbs(T)                                         \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::abs()                           \
{                                                               \
    ippsAbs_##T##_I(ptr, sz);                                   \
    return *this;                                               \
}


#define constructSqr(T)                                         \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::sqr()                           \
{                                                               \
    ippsSqr_##T##_I(ptr, sz);                                   \
    return *this;                                               \
}


#define constructSin(T)                                         \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::sin()                           \
{                                                               \
    ippsSin_##T##_A11(ptr, ptr, sz);                            \
    return *this;                                               \
}


#define constructExp(T)                                         \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::exp()                           \
{                                                               \
    ippsExp_##T##_A11(ptr, ptr, sz);                            \
    return *this;                                               \
}

#define constructInv(T)                                         \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::inv()                           \
{                                                               \
    ippsInv_##T##_A11(ptr, ptr, sz);                            \
    return *this;                                               \
}


#define constructLn(T)                                          \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::ln()                            \
{                                                               \
    ippsLn_##T##_I(ptr, sz);                                    \
    return *this;                                               \
}


#define constructFlip(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::flip()                          \
{                                                               \
    ippsFlip_##T##_I(ptr, sz);                                  \
    return *this;                                               \
}


#define constructFlipBuff(T)                                    \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::flip(Buffer<Ipp##T> buff)       \
{                                                               \
    ippsFlip_##T(buff.ptr, ptr, jmin(buff.sz, sz));             \
    return *this;                                               \
}


#define constructCopy(T)                                        \
template<>                                                      \
void Buffer<Ipp##T>::copyTo(Buffer<Ipp##T> buff) const          \
{                                                               \
    ippsCopy_##T(ptr, buff.ptr, jmin(sz, buff.sz));             \
}


#define constructPhase(T)                                       \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::withPhase(int phase, Buffer<Ipp##T> workBuffer) \
{                                                               \
    if(phase == 0)                                              \
        return *this;                                           \
    section(phase, sz - phase).copyTo(workBuffer);              \
    copyTo(workBuffer.section(sz - phase, phase));              \
    workBuffer.copyTo(*this);                                   \
    return *this;                                               \
}


#define constructRamp(T)                                        \
template<>                                                      \
Buffer<Ipp##T>&  Buffer<Ipp##T>::ramp()                         \
{                                                               \
    if(sz > 1) {                                                \
        Ipp##T delta = 1 / Ipp##T(sz - 1.f);                    \
        ippsVectorSlope_##T(ptr, sz, 0, delta);                 \
    }                                                           \
    return *this;                                               \
}


#define constructRand(T)                                        \
template<>                                                      \
Buffer<Ipp##T>&  Buffer<Ipp##T>::rand(unsigned& seed)           \
{                                                               \
    if(sz > 1) {                                                \
        int size = 0;                                           \
        ippsRandUniformGetSize_##T(&size);                      \
        ScopedAlloc<Ipp8u> stateBuff(size);                     \
        IppsRandUniState_##T* state = (IppsRandUniState_##T*) stateBuff.get(); \
        ippsRandUniformInit_##T(state, -1, 1, seed);            \
        ippsRandUniform_##T(ptr, sz, state);                    \
    }                                                           \
    return *this;                                               \
}


#define constructRampOffset(T)                                  \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::ramp(Ipp##T offset, Ipp##T delta) \
{                                                               \
    if(sz > 0)                                                  \
        if(delta <= 0.000001f && delta >= -0.000001f)           \
            ippsSet_##T(offset, ptr, sz);                       \
        else                                                    \
            ippsVectorSlope_##T(ptr, sz, offset, delta);        \
    return *this;                                               \
}


#define constructNorm(T)                                        \
template<>                                                      \
Ipp##T Buffer<Ipp##T>::normL1()    const                        \
{                                                               \
    if(sz == 0)                                                 \
        return 1.f;                                             \
                                                                \
    Ipp##T norm = 1.f;                                          \
    ippsNorm_L1_##T(ptr, sz, &norm);                            \
    return norm;                                                \
}


#define constructSum(T)                                         \
template<>                                                      \
Ipp##T Buffer<Ipp##T>::sum() const                              \
{                                                               \
    if(sz == 0)                                                 \
        return 0.f;                                             \
                                                                \
    Ipp##T sum = 1.f;                                           \
    ippsSum_##T(ptr, sz, &sum, ippAlgHintFast);                 \
    return sum;                                                 \
}


#define constructMaxVal(T)                                      \
template<>                                                      \
Ipp##T Buffer<Ipp##T>::max() const                              \
{                                                               \
    if(sz == 0)                                                 \
        return 0.f;                                             \
                                                                \
    Ipp##T maxVal = 0.f;                                        \
    ippsMax_##T(ptr, sz, &maxVal);                              \
    return maxVal;                                              \
}

#define constructMinVal(T)                                      \
template<>                                                      \
Ipp##T Buffer<Ipp##T>::min() const                              \
{                                                               \
    if(sz == 0)                                                 \
        return 0.f;                                             \
                                                                \
    Ipp##T minVal = 0.f;                                        \
    ippsMin_##T(ptr, sz, &minVal);                              \
    return minVal;                                              \
}


#define constructMean(T)                                        \
template<>                                                      \
Ipp##T Buffer<Ipp##T>::mean() const                             \
{                                                               \
    if(sz == 0)                                                 \
        return 0.f;                                             \
                                                                \
    Ipp##T mean = 0.f;                                          \
    ippsMean_##T(ptr, sz, &mean, ippAlgHintFast);               \
    return mean;                                                \
}


#define constructMinMax(T)                                      \
template<>                                                      \
void Buffer<Ipp##T>::minmax(Ipp##T& pMin, Ipp##T& pMax) const   \
{                                                               \
    ippsMinMax_##T(ptr, sz, &pMin, &pMax);                      \
}


#define constructNormL2(T)                                      \
template<>                                                      \
Ipp##T Buffer<Ipp##T>::normL2()    const                        \
{                                                               \
    if(sz == 0)                                                 \
        return 1.f;                                             \
                                                                \
    Ipp##T norm = 1.f;                                          \
    ippsNorm_L2_##T(ptr, sz, &norm);                            \
    return norm;                                                \
}


#define constructNormDiffL2(T)                                  \
template<>                                                      \
Ipp##T Buffer<Ipp##T>::normDiffL2(Buffer<Ipp##T> buff) const    \
{                                                               \
    if(sz == 0)                                                 \
        return 1.f;                                             \
                                                                \
    Ipp##T norm = 1.f;                                          \
    ippsNormDiff_L2_##T(buff.ptr, ptr, sz, &norm);              \
    return norm;                                                \
}


#define constructDot(T)                                         \
template<>                                                      \
Ipp##T Buffer<Ipp##T>::dot(Buffer<Ipp##T> buff)    const        \
{                                                               \
    if(sz == 0)                                                 \
        return 0.f;                                             \
                                                                \
    Ipp##T prod = 1.f;                                          \
    ippsDotProd_##T(ptr, buff.ptr, jmin(sz, buff.sz), &prod);   \
    return prod;                                                \
}


#define constructMax(T)                                         \
template<>                                                      \
void Buffer<Ipp##T>::getMax(Ipp##T& pMax, int& idx) const       \
{                                                               \
    if(sz == 0)                                                 \
        return;                                                 \
                                                                \
    ippsMaxIndx_##T(ptr, sz, &pMax, &idx);                      \
}

#define constructMin(T)                                         \
template<>                                                      \
void Buffer<Ipp##T>::getMin(Ipp##T& pMin, int& idx)    const    \
{                                                               \
    if(sz == 0)                                                 \
        return;                                                 \
                                                                \
    ippsMinIndx_##T(ptr, sz, &pMin, &idx);                      \
}


#define constructCombineC(T)                                    \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::addProduct(Buffer<Ipp##T> buff, Ipp##T c) \
{                                                               \
    if(sz == 0 || c == Ipp##T(0))                               \
        return *this;                                           \
    if(c == Ipp##T(1))                                          \
        add(buff);                                              \
    else                                                        \
        ippsAddProductC_##T(buff.ptr, c, ptr, jmin(sz, buff.sz)); \
    return *this;                                               \
}

#define constructCombine(T)                                     \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::addProduct(Buffer<Ipp##T> src1, \
                                           Buffer<Ipp##T> src2) \
{                                                               \
    if(sz > 0)                                                  \
        ippsAddProduct_##T(src1.ptr, src2.ptr, ptr, jmin(sz, src1.sz, src2.sz)); \
    return *this;                                               \
}


#define constructThreshLT(T)                                    \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::threshLT(Ipp##T c)              \
{                                                               \
    if(sz > 0)                                                  \
        ippsThreshold_LT_##T##_I(ptr, sz, c);                   \
    return *this;                                               \
}


#define constructThreshGT(T)                                    \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::threshGT(Ipp##T c)              \
{                                                               \
    if(sz > 0)                                                  \
        ippsThreshold_GT_##T##_I(ptr, sz, c);                   \
    return *this;                                               \
}

#define constructDownsample(T)                                  \
template<>                                                      \
int Buffer<Ipp##T>::downsampleFrom(Buffer<Ipp##T> buff, int factor, int phase) \
{                                                               \
    if(factor < 0)                                              \
        factor = buff.size() / sz;                              \
    if(sz == 0)                                                 \
        return 0;                                               \
                                                                \
    if(factor == 1) {                                           \
        buff.copyTo(*this);                                     \
        return 0;                                               \
    }                                                           \
                                                                \
    int destSize;                                               \
                                                                \
    ippsSampleDown_##T(buff.ptr, buff.size(), ptr,              \
                       &destSize, factor, &phase);              \
    return phase;                                               \
}

#define constructUpsample(T)                                    \
template<>                                                      \
int Buffer<Ipp##T>::upsampleFrom(Buffer<Ipp##T> buff, int factor, int phase) \
{                                                               \
    if(factor < 0)                                              \
        factor = buff.size() / sz;                              \
    if(sz == 0)                                                 \
        return 0;                                               \
                                                                \
    if(factor == 1) {                                           \
        buff.copyTo(*this);                                     \
        return 0;                                               \
    }                                                           \
                                                                \
    int destSize;                                               \
                                                                \
    ippsSampleUp_##T(buff.ptr, buff.size(), ptr, &destSize, factor, &phase); \
    return phase;                                               \
}

#define implementOperators(T) \
template<> \
void Buffer<Ipp##T>::operator+=(const Buffer<Ipp##T>& other) { \
    add(other);                                                \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<Ipp##T>::operator+=(Ipp##T val) {                  \
    add(val);                                                  \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<Ipp##T>::operator-=(const Buffer<Ipp##T>& other) { \
    sub(other);                                                \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<Ipp##T>::operator-=(Ipp##T val) {                  \
    sub(val);                                                  \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<Ipp##T>::operator*=(const Buffer<Ipp##T>& other) { \
    mul(other);                                                \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<Ipp##T>::operator*=(Ipp##T val) {                  \
    mul(val);                                                  \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<Ipp##T>::operator/=(const Buffer<Ipp##T>& other) { \
    div(other);                                                \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<Ipp##T>::operator/=(Ipp##T val) {                  \
    div(val);                                                  \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<Ipp##T>::operator<<(const Buffer<Ipp##T>& other) { \
    other.copyTo(*this);                                       \
}                                                              \
                                                               \
template<>                                                     \
void Buffer<Ipp##T>::operator>>(Buffer<Ipp##T> other) const {  \
    copyTo(other);                                             \
}


declareForCommonTypes(constructZeroSz);
declareForCommonTypes(constructZero);
declareForCommonTypes(constructSet);
declareForCommonTypes(constructCopy);
declareForCommonTypes(constructPhase);
declareForRealAndCplx(constructAdd);
declareForRealAndCplx(constructDiv);
declareForRealAndCplx(constructSub);
declareForRealAndCplx(constructAdd2);
declareForRealAndCplx(constructMulComb2);
declareForFloatAndCplx(constructCombine);
declareForReal(constructAddC);
declareForReal(constructAddBuffC);
declareForReal(constructMul);
declareForReal(constructMulC);
declareForReal(constructDivC);
declareForReal(constructMulComb);
declareForReal(constructSubC);
declareForReal(constructSubCRev);
declareForReal(constructRamp);
declareForReal(constructRampOffset);
declareForReal(constructNorm);
declareForReal(constructNormL2);
declareForReal(constructAbs);
declareForReal(constructMax);
declareForReal(constructThreshLT);
declareForReal(constructThreshGT);

constructNormDiffL2(32f);
constructDownsample(32f);
constructUpsample(32f);
constructSubCRevB(32f);
constructSqrt(32f);
constructDot(32f);
constructCombineC(32f);
constructDivCRev(32f);
constructSqr(32f);
constructSin(32f);
constructExp(32f);
constructInv(32f);
constructLn(32f);
constructSum(32f);
constructSubB(32f);
constructFlip(32f);
constructFlipBuff(32f);
constructMin(32f);
constructMaxVal(32f);
constructMinVal(32f);
constructMinMax(32f);
constructConv(32f);
constructRand(32f);
constructMean(32f);
constructStddev(32f);
constructSort(32f);
constructDiff(32f);

template<>
Buffer<Ipp32f>& Buffer<Ipp32f>::tanh() {
    ippsTanh_32f_A24(ptr, ptr, sz);
    return *this;
}

template<>
Buffer<Ipp32f>& Buffer<Ipp32f>::pow(Ipp32f c) {
    if(c != 1.f)
        ippsPowx_32f_A11(ptr, c, ptr, sz);
    return *this;
}

template<>
Buffer<Ipp32fc>& Buffer<Ipp32fc>::mul(Buffer buff, Ipp32fc c) {
    if (c.re == 1 && c.im == 0) {
        buff.copyTo(*this);
    } else {
        ippsMulC_32fc(buff.ptr, c, ptr, jmin(sz, buff.sz));
    }
    return *this;
}

implementOperators(32f)
implementOperators(64f)

// template<>
// Buffer<Ipp32f>& Buffer<Ipp32f>::conv(
//     Buffer<Ipp32f> src1,
//     Buffer<Ipp32f> src2,
//     Buffer<Ipp8u> workBuff) {
//     do {
//         if (!(sz >= src1.size() + src2.size() - 1)) do {
//             juce::logAssertion("Buffer. cpp", 692);;
//             if (juce::juce_isRunningUnderDebugger()) { ::kill(0, 5); };
//         } while (false);
//     } while (false);
//     ippsConvolve_32f(src1.get(), src1.sz, src2.get(), src2.sz, ptr, IppAlgType::ippAlgAuto, workBuff.get());
//     return *this;
// }

/*
#ifdef USE_IPP
    using Int32 = Ipp32s;
    using Float32 = Ipp32f;
    using Float64 = Ipp64f;
#else
    using Int32 = int32_t;
    using Float32 = float;
    using Float64 = double;
#endif

template<typename T>
struct NumericTraits {
    static constexpr const char* suffix = nullptr;
    static constexpr const char* accel_prefix = nullptr;
};

template<>
struct NumericTraits<Float32> {
    static constexpr const char* suffix = "32f";
    static constexpr const char* accel_prefix = "";
};

template<>
struct NumericTraits<Float64> {
    static constexpr const char* suffix = "64f";
    static constexpr const char* accel_prefix = "D";
};

#define constructSet(VTYPE)                                     \
template<>                                                      \
Buffer<VTYPE>& Buffer<VTYPE>::set(VTYPE value)                  \
{                                                               \
    if (sz == 0) return *this;                                  \
      #ifdef USE_ACCELERATE                                     \
        vDSP_vfill##NumericTraits<VTYPE>::accel_prefix(&value, ptr, 1, sz); \
      #else                                                     \
        ippsSet_##NumericTraits<VTYPE>::suffix(value, ptr, sz); \
      #endif                                                    \
    return *this;                                               \
}

// Usage:
constructSet(Float32)
constructSet(Float64)

// Basic Operations
ippsAdd_32f_I(a, b, len)           -> vDSP_vadd(a, 1, b, 1, dst, 1, len)
ippsSub_32f_I(a, b, len)           -> vDSP_vsub(a, 1, b, 1, dst, 1, len)
ippsMul_32f_I(a, b, len)           -> vDSP_vmul(a, 1, b, 1, dst, 1, len)
ippsDiv_32f_I(a, b, len)           -> vDSP_vdiv(a, 1, b, 1, dst, 1, len)

// Constants
ippsSet_32f(val, dst, len)         -> vDSP_vfill(&val, dst, 1, len)
ippsMulC_32f_I(val, dst, len)      -> vDSP_vsmul(dst, 1, &val, dst, 1, len)
ippsAddC_32f_I(val, dst, len)      -> vDSP_vsadd(dst, 1, &val, dst, 1, len)
ippsSubC_32f_I(val, dst, len)      -> vDSP_vsadd(dst, 1, &(-val), dst, 1, len)
ippsDivC_32f_I(val, dst, len)      -> vDSP_vsdiv(dst, 1, &val, dst, 1, len)

// Statistics
ippsMean_32f(src, len, &mean)      -> vDSP_meanv(src, 1, &mean, len)
ippsMax_32f(src, len, &max)        -> vDSP_maxv(src, 1, &max, len)
ippsMin_32f(src, len, &min)        -> vDSP_minv(src, 1, &min, len)
ippsStdDev_32f(src, len, &std)     -> vDSP_normalize(src, 1, dst, 1, &mean, &std, len)

// Complex Operations
ippsConvolve_32f()                 -> vDSP_conv(src, 1, kernel, 1, dst, 1, len, kernelLen)
ippsFlip_32f_I(ptr, len)           -> vDSP_vrvrs(ptr, 1, len)
ippsDotProd_32f(a, b, len, &dot)   -> vDSP_dotpr(a, 1, b, 1, &dot, len)

// Double Precision
// Add _D suffix for 64-bit versions:
vDSP_vadd -> vDSP_vaddD
vDSP_vmul -> vDSP_vmulD

// Basic Math
ippsZero_32f(ptr, len)             -> vDSP_vclr(ptr, 1, len)
ippsSqrt_32f(src, dst, len)        -> vvsqrtf(dst, src, &len)
ippsAbs_32f_I(ptr, len)            -> vDSP_vabs(ptr, 1, dst, 1, len)
ippsSin_32f(src, dst, len)         -> vvsinf(dst, src, &len)
ippsExp_32f(src, dst, len)         -> vvexpf(dst, src, &len)
ippsLn_32f(src, dst, len)          -> vvlogf(dst, src, &len)
ippsInv_32f(src, dst, len)         -> vvrecf(dst, src, &len)
ippsTanh_32f(src, dst, len)        -> vvtanhf(dst, src, &len)

// Vector Operations
ippsVectorSlope_32f(dst, len, offset, delta) -> vDSP_vramp(&offset, &delta, dst, 1, len)

// Norms and Sums
ippsNorm_L1_32f(src, len, &norm)   -> vDSP_svemg(src, 1, &norm, len)
ippsNorm_L2_32f(src, len, &norm)   -> vDSP_svesq(src, 1, &norm, len)
ippsSum_32f(src, len, &sum)        -> vDSP_sve(src, 1, &sum, len)

// Index Operations
ippsMaxIndx_32f(src, len, &max, &idx) -> vDSP_maxvi(src, 1, &max, &idx, len)
ippsMinIndx_32f(src, len, &min, &idx) -> vDSP_minvi(src, 1, &min, &idx, len)

// Threshold Operations
ippsThreshold_LT_32f_I(ptr, len, thresh) -> vDSP_vthr(ptr, 1, &thresh, dst, 1, len)
ippsThreshold_GT_32f_I(ptr, len, thresh) -> vDSP_vthres(ptr, 1, &thresh, dst, 1, len)
 */