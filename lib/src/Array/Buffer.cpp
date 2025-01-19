#include "Buffer.h"

#include "ArrayDefs.h"

#ifdef USE_IPP
#include "ScopedAlloc.h"
#include "ipp.h"

#pragma warning(disable: 4661)

#define declareForCommonTypes(T) \
    T(8u)   \
    T(16s)  \
    T(32s)  \
    T(32f) \
    T(64f) \
    T(32fc)

#define declareForReal(T) \
    T(32f) \
    T(64f)

#define declareForRealPrec(T) \
    T(32f, A24) \
    T(64f, A50)

#define declareForRealAndCplx(T) \
    T(32f)  \
    T(64f)  \
    T(32fc)

#define declareForFloatAndCplx(T) \
    T(32f)  \
    T(32fc)

template class Buffer<Int8u>;
template class Buffer<Int8s>;
template class Buffer<Int16s>;
template class Buffer<Int32s>;
template class Buffer<Float32>;
template class Buffer<Float64>;
template class Buffer<Complex32>;

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

defineAudioBufferConstructor(Float32)
defineAudioBufferConstructor(Float64)

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

#define constructSubCRev(T)                                     \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::subCRev(Ipp##T c)               \
{                                                               \
    ippsSubCRev_##T##_I(c, ptr, sz);                            \
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

#define constructTanh(T, prec)                                  \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::tanh()                          \
{                                                               \
    ippsTanh_##T##_##prec(ptr, ptr, sz);                        \
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

#define constructSqrt(T, prec)                                  \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::sqrt()                          \
{                                                               \
    ippsSqrt_##T##_##prec(ptr, ptr, sz);                        \
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

#define constructSin(T, prec)                                   \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::sin()                           \
{                                                               \
    ippsSin_##T##_##prec(ptr, ptr, sz);                         \
    return *this;                                               \
}

#define constructExp(T, prec)                                   \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::exp()                           \
{                                                               \
    ippsExp_##T##_##prec(ptr, ptr, sz);                         \
    return *this;                                               \
}

#define constructInv(T, prec)                                   \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::inv()                           \
{                                                               \
    ippsInv_##T##_##prec(ptr, ptr, sz);                         \
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
Buffer<Ipp##T>& Buffer<Ipp##T>::withPhase(int phase,            \
        Buffer<Ipp##T> workBuffer) {                            \
    if(phase == 0 || sz == 0)                                   \
        return *this;                                           \
    phase = phase % sz;                                         \
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
        ScopedAlloc<Int8u> stateBuff(size);                     \
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

#define constructPow(T, prec)                                   \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::pow(Ipp##T c) {                 \
    if(c != 1.)                                                 \
        ippsPowx_##T##_##prec(ptr, c, ptr, sz);                 \
    return *this;                                               \
}

#define constructPowCRev(T, prec)                               \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::powCRev(Ipp##T k) {             \
    if(sz == 0 || k == 1) return *this;                         \
    Ipp##T c = std::log(k);                                     \
    ippsMulC_##T##_I(c, ptr, sz);                               \
    ippsExp_##T##_##prec(ptr, ptr, sz);                         \
    return *this;                                               \
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

#define constructClip(T)                                        \
template<>                                                      \
Buffer<Ipp##T>& Buffer<Ipp##T>::clip(Ipp##T low, Ipp##T high)   \
{                                                               \
    if(sz > 0)                                                  \
        ippsThreshold_LTValGTVal_##T##_I(ptr, sz, low, low, high, high); \
    return *this;                                               \
}

#define constructDownsample(T)                                  \
template<>                                                      \
int Buffer<Ipp##T>::downsampleFrom(Buffer<Ipp##T> buff, int factor, int phase) \
{                                                               \
    if(sz == 0 || buff.empty())                                 \
        return 0;                                               \
    if(factor < 0)                                              \
        factor = buff.size() / sz;                              \
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
    if(sz == 0 || buff.empty())                                 \
        return 0;                                               \
    if(factor < 0)                                              \
        factor = sz / buff.size();                              \
                                                                \
    if(factor == 1) {                                           \
        buff.copyTo(*this);                                     \
        return 0;                                               \
    }                                                           \
                                                                \
    int destSize;                                               \
                                                                \
    ippsSampleUp_##T(                                           \
        buff.ptr, buff.size(),                                  \
        ptr, &destSize, factor, &phase                          \
    );                                                          \
    return phase;                                               \
}

#define constructWinHann(T)                      \
    template<>                                   \
    Buffer<Ipp##T>& Buffer<Ipp##T>::hann() {     \
        if(sz == 0) return *this;                \
        ippsWinHann_##T(ptr, ptr, sz);           \
        return *this;                            \
    }

#define constructWinBlackman(T)                  \
    template<>                                   \
    Buffer<Ipp##T>& Buffer<Ipp##T>::blackman() { \
        if(sz == 0) return *this;                \
        ippsWinBlackmanStd_##T(ptr, ptr, sz);    \
        return *this;                            \
    }

#define constructSinInit(T) \
    template<> \
    Buffer<Ipp##T>& Buffer<Ipp##T>::sin(float relFreq, float unitPhase) { \
        if(sz == 0) return *this; \
        return ramp( \
                static_cast<Ipp##T>(unitPhase * 2 * M_PI), \
                static_cast<Ipp##T>(relFreq * 2 * M_PI) \
            ).sin(); \
    }

#define implementOperators(T)                                  \
template<>                                                     \
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
declareForFloatAndCplx(constructCombine);
declareForReal(constructAddC);
declareForReal(constructMul);
declareForReal(constructMulC);
declareForReal(constructDivC);
declareForReal(constructSubC);
declareForReal(constructSubCRev);
declareForReal(constructRamp);
declareForReal(constructRampOffset);
declareForReal(constructNorm);
declareForReal(constructNormL2);
declareForReal(constructAbs);
declareForReal(constructClip);
declareForReal(constructMax);
declareForReal(constructThreshLT);
declareForReal(constructThreshGT);
declareForReal(constructWinHann);
declareForReal(constructWinBlackman);
declareForReal(constructFlip);

declareForRealPrec(constructSqrt)
declareForRealPrec(constructPow)
declareForRealPrec(constructInv)
declareForRealPrec(constructExp)
declareForRealPrec(constructSin)
declareForRealPrec(constructTanh)
declareForRealPrec(constructPowCRev)

declareForReal(constructSinInit);

constructNormDiffL2(32f);
constructDownsample(32f);
constructUpsample(32f);
constructDot(32f);
constructCombineC(32f);
constructSqr(32f);
constructLn(32f);
constructSum(32f);
constructDivCRev(32f);

constructMin(32f);
constructMaxVal(32f);
constructMinVal(32f);
constructMinMax(32f);
constructRand(32f);
constructMean(32f);
constructStddev(32f);
constructSort(32f);

implementOperators(32f)
implementOperators(64f)

#endif