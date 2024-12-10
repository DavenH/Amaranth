#include "Buffer.h"
#include "ScopedAlloc.h"

#include "ippcv.h"
#include "ipps.h"
#include "ippvm.h"

#pragma warning(disable: 4661)

#define declareForCommonTypes(T)	\
	T(8u)	\
	T(16s)	\
	T(32s)	\
	T(32f)	\
	T(64f)	\
	T(32fc)

#define declareForReal(T)	\
	T(32f)	\
	T(64f)

#define declareForRealAndCplx(T)	\
	T(32f)	\
	T(64f)	\
	T(32fc)

#define declareForFloatAndCplx(T)	\
	T(32f)	\
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
	int size 	 = sz / 2;

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

#define constructStddev(T)										\
template<>														\
Ipp##T Buffer<Ipp##T>::stddev()	const							\
{																\
	Ipp##T deviation;											\
	ippsStdDev_##T(ptr, sz, &deviation, ippAlgHintFast);		\
	return deviation;											\
}

#define constructSort(T)										\
template<>														\
Buffer<Ipp##T>& Buffer<Ipp##T>::sort()							\
{																\
	ippsSortAscend_##T##_I(ptr, sz);							\
	return *this;												\
}

#define constructZero(T)										\
template<>														\
Buffer<Ipp##T>& Buffer<Ipp##T>::zero()							\
{																\
	ippsZero_##T(ptr, sz);										\
	return *this;												\
}

#define constructZeroSz(T)										\
template<>														\
Buffer<Ipp##T>& Buffer<Ipp##T>::zero(int size)					\
{																\
	if(sz == 0)													\
		return *this;											\
	jassert(size <= sz);										\
	ippsZero_##T(ptr, size);									\
	return *this;												\
}

#define constructSet(T)											\
template<>														\
Buffer<Ipp##T>& Buffer<Ipp##T>::set(Ipp##T value)				\
{																\
	ippsSet_##T(value, ptr, sz);								\
	return *this;												\
}

#define constructAdd(T)											\
template<>														\
Buffer<Ipp##T>& Buffer<Ipp##T>::add(Buffer<Ipp##T> buff)		\
{																\
	ippsAdd_##T##_I(buff.ptr, ptr, jmin(sz, buff.sz));			\
	return *this;												\
}

#define constructAdd2(T)														\
template<>																		\
Buffer<Ipp##T>& Buffer<Ipp##T>::add(Buffer<Ipp##T> src1, Buffer<Ipp##T> src2)	\
{																				\
	ippsAdd_##T(src1.ptr, src2.ptr, ptr, jmin(sz, src1.sz, src2.sz));			\
	return *this;																\
}

#define constructAddC(T)												\
template<>																\
Buffer<Ipp##T>& Buffer<Ipp##T>::add(Ipp##T c)							\
{																		\
	if(c != Ipp##T())													\
		ippsAddC_##T##_I(c, ptr, sz);									\
	return *this;														\
}

#define constructAddBuffC(T)											\
template<>																\
Buffer<Ipp##T>& Buffer<Ipp##T>::add(Buffer<Ipp##T> buff, Ipp##T c)		\
{																		\
	if(c == Ipp##T())													\
		ippsAdd_##T##_I(buff.ptr, ptr, jmin(buff.sz,sz));				\
	else																\
		ippsAddC_##T(buff.ptr, c, ptr, jmin(buff.sz,sz));				\
	return *this;														\
}

#define constructSub(T)													\
template<>																\
Buffer<Ipp##T>& Buffer<Ipp##T>::sub(Buffer<Ipp##T> buff)				\
{																		\
	ippsSub_##T##_I(buff.ptr, ptr, jmin(sz, buff.sz));					\
	return *this;														\
}

#define constructDiff(T)												\
template<>																\
Buffer<Ipp##T>& Buffer<Ipp##T>::diff(Buffer<Ipp##T> buff)				\
{																		\
	if(sz < 2)															\
		return *this;													\
	ippsSub_##T(buff.ptr, buff.ptr + 1, ptr, jmin(buff.sz - 1, sz - 1));\
	ptr[sz - 1] = ptr[sz - 2];											\
	return *this;														\
}

#define constructSubB(T)														\
template<>																		\
Buffer<Ipp##T>& Buffer<Ipp##T>::sub(Buffer<Ipp##T> src1, Buffer<Ipp##T> src2)	\
{																				\
	ippsSub_##T(src1.ptr, src2.ptr, ptr, jmin(sz, src1.sz, src2.sz));			\
	return *this;																\
}

#define constructSubCRev(T)											\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::subCRev(Ipp##T c)					\
{																	\
	ippsSubCRev_##T##_I(c, ptr, sz);								\
	return *this;													\
}

#define constructSubCRevB(T)										\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::subCRev(Ipp##T c, Buffer<Ipp##T> buff)	\
{																	\
	ippsSubCRev_##T(buff.ptr, c, ptr, jmin(sz, buff.sz));			\
	return *this;													\
}

#define constructDivCRev(T)										\
template<>														\
Buffer<Ipp##T>& Buffer<Ipp##T>::divCRev(Ipp##T c)				\
{																\
	ippsDivCRev_##T##_I(c, ptr, sz);							\
	return *this;												\
}

#define constructMul(T)											\
template<>														\
Buffer<Ipp##T>& Buffer<Ipp##T>::mul(Buffer<Ipp##T> buff)		\
{																\
	ippsMul_##T##_I(buff.ptr, ptr, jmin(sz, buff.sz));			\
	return *this;												\
}

#define constructDiv(T)											\
template<>														\
Buffer<Ipp##T>& Buffer<Ipp##T>::div(Buffer<Ipp##T> buff)		\
{																\
	ippsDiv_##T##_I(buff.ptr, ptr, jmin(sz, buff.sz));			\
	return *this;												\
}

#define constructMulC(T)										\
template<>														\
Buffer<Ipp##T>& Buffer<Ipp##T>::mul(Ipp##T c)					\
{																\
	if(c != 1)													\
		ippsMulC_##T##_I(c, ptr, sz);							\
	return *this;												\
}

template<> Buffer<Ipp32fc>& Buffer<Ipp32fc>::mul(Ipp32fc c)
{
	if(c.re != 1 || c.im != 0)
		ippsMulC_32fc_I(c, ptr, sz);
	return *this;
}

#define constructMulComb(T)											\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::mul(Buffer<Ipp##T> buff, Ipp##T c)	\
{																	\
	if(c == Ipp##T(1)) {											\
		buff.copyTo(*this);											\
	} else {														\
		ippsMulC_##T(buff.ptr, c, ptr, jmin(sz, buff.sz));			\
	}																\
	return *this;													\
}

#define constructMulComb2(T)													\
template<>																		\
Buffer<Ipp##T>& Buffer<Ipp##T>::mul(Buffer<Ipp##T> src1, Buffer<Ipp##T> src2)	\
{																				\
	ippsMul_##T(src1.ptr, src2.ptr, ptr, jmin(sz, src1.sz, src2.sz));			\
	return *this;																\
}


#define constructConv(T)																				\
template<>																								\
Buffer<Ipp##T>& Buffer<Ipp##T>::conv(Buffer<Ipp##T> src1, Buffer<Ipp##T> src2, Buffer<Ipp8u> workBuff)	\
{																										\
	jassert(sz >= src1.size() + src2.size() - 1);														\
	ippsConvolve_##T(src1, src1.sz, src2, src2.sz, ptr, IppAlgType::ippAlgAuto, workBuff);				\
	return *this;																						\
}


#define constructSqrt(T)											\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::sqrt()								\
{																	\
	ippsSqrt_##T##_A11(ptr, ptr, sz);								\
	return *this;													\
}


#define constructAbs(T)												\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::abs()								\
{																	\
	ippsAbs_##T##_I(ptr, sz);										\
	return *this;													\
}


#define constructSqr(T)												\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::sqr()								\
{																	\
	ippsSqr_##T##_I(ptr, sz);										\
	return *this;													\
}


#define constructSin(T)												\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::sin()								\
{																	\
	ippsSin_##T##_A11(ptr, ptr, sz);								\
	return *this;													\
}


#define constructExp(T)												\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::exp()								\
{																	\
	ippsExp_##T##_A11(ptr, ptr, sz);								\
	return *this;													\
}

#define constructInv(T)												\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::inv()								\
{																	\
	ippsInv_##T##_A11(ptr, ptr, sz);								\
	return *this;													\
}


#define constructLn(T)												\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::ln()								\
{																	\
	ippsLn_##T##_I(ptr, sz);										\
	return *this;													\
}


#define constructFlip(T)											\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::flip()								\
{																	\
	ippsFlip_##T##_I(ptr, sz);										\
	return *this;													\
}


#define constructFlipBuff(T)										\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::flip(Buffer<Ipp##T> buff)			\
{																	\
	ippsFlip_##T(buff.ptr, ptr, jmin(buff.sz, sz));					\
	return *this;													\
}


#define constructCopy(T)											\
template<>															\
void Buffer<Ipp##T>::copyTo(Buffer<Ipp##T> buff) const				\
{																	\
	ippsCopy_##T(ptr, buff.ptr, jmin(sz, buff.sz));					\
}


#define constructPhase(T)											\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::withPhase(int phase, Buffer<Ipp##T> workBuffer) 	\
{																	\
	if(phase == 0)													\
		return *this;												\
	section(phase, sz - phase).copyTo(workBuffer);					\
	copyTo(workBuffer.section(sz - phase, phase));					\
	workBuffer.copyTo(*this);										\
	return *this;													\
}


#define constructRamp(T)											\
template<>															\
Buffer<Ipp##T>&  Buffer<Ipp##T>::ramp()								\
{																	\
	if(sz > 1) {													\
		Ipp##T delta = 1 / Ipp##T(sz - 1.f);						\
		ippsVectorSlope_##T(ptr, sz, 0, delta);						\
	}																\
	return *this;													\
}


#define constructRand(T)											\
template<>															\
Buffer<Ipp##T>&  Buffer<Ipp##T>::rand(unsigned& seed)				\
{																	\
	if(sz > 1) {													\
		int size = 0;												\
		ippsRandUniformGetSize_##T(&size);							\
		ScopedAlloc<Ipp8u> stateBuff(size);							\
		IppsRandUniState_##T* state = (IppsRandUniState_##T*) stateBuff.get();	\
		ippsRandUniformInit_##T(state, -1, 1, seed);				\
		ippsRandUniform_##T(ptr, sz, state);						\
	}																\
	return *this;													\
}


#define constructRampOffset(T)										\
template<>															\
Buffer<Ipp##T>& Buffer<Ipp##T>::ramp(Ipp##T offset, Ipp##T delta)	\
{																	\
	if(sz > 0)														\
		ippsVectorSlope_##T(ptr, sz, offset, delta);				\
	return *this;													\
}


#define constructNorm(T)											\
template<>															\
Ipp##T Buffer<Ipp##T>::normL1()	const  								\
{																	\
	if(sz == 0)														\
		return 1.f;													\
																	\
	Ipp##T norm = 1.f;												\
	ippsNorm_L1_##T(ptr, sz, &norm);								\
	return norm;													\
}


#define constructSum(T)												\
template<>															\
Ipp##T Buffer<Ipp##T>::sum() const  								\
{																	\
	if(sz == 0)														\
		return 0.f;													\
																	\
	Ipp##T sum = 1.f;												\
	ippsSum_##T(ptr, sz, &sum, ippAlgHintFast);						\
	return sum;														\
}


#define constructMaxVal(T)											\
template<>															\
Ipp##T Buffer<Ipp##T>::max() const									\
{																	\
	if(sz == 0)														\
		return 0.f;													\
																	\
	Ipp##T maxVal = 0.f;											\
	ippsMax_##T(ptr, sz, &maxVal);									\
	return maxVal;													\
}


#define constructMinVal(T)											\
template<>															\
Ipp##T Buffer<Ipp##T>::min() const									\
{																	\
	if(sz == 0)														\
		return 0.f;													\
																	\
	Ipp##T minVal = 0.f;											\
	ippsMin_##T(ptr, sz, &minVal);									\
	return minVal;													\
}


#define constructMean(T)											\
template<>															\
Ipp##T Buffer<Ipp##T>::mean() const									\
{																	\
	if(sz == 0)														\
		return 0.f;													\
																	\
	Ipp##T mean = 0.f;												\
	ippsMean_##T(ptr, sz, &mean, ippAlgHintFast);					\
	return mean;													\
}


#define constructMinMax(T) 											\
template<>															\
void Buffer<Ipp##T>::minmax(Ipp##T& pMin, Ipp##T& pMax)	const		\
{																	\
	ippsMinMax_##T(ptr, sz, &pMin, &pMax);							\
}


#define constructNormL2(T)											\
template<>															\
Ipp##T Buffer<Ipp##T>::normL2()	const								\
{																	\
	if(sz == 0)														\
		return 1.f;													\
																	\
	Ipp##T norm = 1.f;												\
	ippsNorm_L2_##T(ptr, sz, &norm);								\
	return norm;													\
}


#define constructNormDiffL2(T)										\
template<>															\
Ipp##T Buffer<Ipp##T>::normDiffL2(Buffer<Ipp##T> buff) const		\
{																	\
	if(sz == 0)														\
		return 1.f;													\
																	\
	Ipp##T norm = 1.f;												\
	ippsNormDiff_L2_##T(buff.ptr, ptr, sz, &norm);					\
	return norm;													\
}


#define constructDot(T)												\
template<>															\
Ipp##T Buffer<Ipp##T>::dot(Buffer<Ipp##T> buff)	const				\
{																	\
	if(sz == 0)														\
		return 0.f;													\
																	\
	Ipp##T prod = 1.f;												\
	ippsDotProd_##T(ptr, buff.ptr, jmin(sz, buff.sz), &prod);		\
	return prod;													\
}


#define constructMax(T)												\
template<>															\
void Buffer<Ipp##T>::getMax(Ipp##T& pMax, int& idx) const			\
{																	\
	if(sz == 0)														\
		return;														\
																	\
	ippsMaxIndx_##T(ptr, sz, &pMax, &idx);							\
}


#define constructMin(T)												\
template<>															\
void Buffer<Ipp##T>::getMin(Ipp##T& pMin, int& idx)	const			\
{																	\
	if(sz == 0)														\
		return;														\
																	\
	ippsMinIndx_##T(ptr, sz, &pMin, &idx);							\
}


#define constructCombineC(T)												\
template<>																	\
Buffer<Ipp##T>& Buffer<Ipp##T>::addProduct(Buffer<Ipp##T> buff, Ipp##T c)	\
{																			\
	if(sz == 0 || c == Ipp##T(0))											\
		return *this;														\
	if(c == Ipp##T(1))														\
		add(buff);															\
	else																	\
		ippsAddProductC_##T(buff.ptr, c, ptr, jmin(sz, buff.sz));			\
	return *this;															\
}

#define constructCombine(T)													\
template<>																	\
Buffer<Ipp##T>& Buffer<Ipp##T>::addProduct(Buffer<Ipp##T> src1, 			\
										   Buffer<Ipp##T> src2)				\
{																			\
	if(sz > 0)																\
		ippsAddProduct_##T(src1.ptr, src2.ptr, ptr, jmin(sz, src1.sz, src2.sz)); \
	return *this;															\
}


#define constructThreshLT(T)												\
template<>																	\
Buffer<Ipp##T>& Buffer<Ipp##T>::threshLT(Ipp##T c)							\
{																			\
	if(sz > 0)																\
		ippsThreshold_LT_##T##_I(ptr, sz, c);								\
	return *this;															\
}


#define constructThreshGT(T)												\
template<>																	\
Buffer<Ipp##T>& Buffer<Ipp##T>::threshGT(Ipp##T c)							\
{																			\
	if(sz > 0)																\
		ippsThreshold_GT_##T##_I(ptr, sz, c);								\
	return *this;															\
}

#define constructDownsample(T)													\
template<>																		\
int Buffer<Ipp##T>::downsampleFrom(Buffer<Ipp##T> buff, int factor, int phase)	\
{																				\
	if(factor < 0)																\
		factor = buff.size() / sz;												\
	if(sz == 0)																	\
		return 0;																\
																				\
	if(factor == 1) {															\
		buff.copyTo(*this);														\
		return 0;																\
	}																			\
																				\
	int destSize;																\
																				\
	ippsSampleDown_##T(buff.ptr, buff.size(), ptr, 								\
					   &destSize, factor, &phase);								\
	return phase;																\
}

#define constructUpsample(T)													\
template<>																		\
int Buffer<Ipp##T>::upsampleFrom(Buffer<Ipp##T> buff, int factor, int phase)	\
{																				\
	if(factor < 0)																\
		factor = buff.size() / sz;												\
	if(sz == 0)																	\
		return 0;																\
																				\
	if(factor == 1) {															\
		buff.copyTo(*this);														\
		return 0;																\
	}																			\
																				\
	int destSize;																\
																				\
	ippsSampleUp_##T(buff.ptr, buff.size(), ptr, &destSize, factor, &phase);	\
	return phase;																\
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
declareForReal(constructMulComb);
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
//constructDiv(32f);
constructSqr(32f);
constructSin(32f);
constructExp(32f);
constructInv(32f);
constructLn(32f);
constructSum(32f);
//constructSub(32f);
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
Buffer<Ipp32f>& Buffer<Ipp32f>::pow(Ipp32f c) {
	if(c != 1.f)
		ippsPowx_32f_A11(ptr, c, ptr, sz);
	return *this;
}


template<>
Buffer<Ipp32fc>& Buffer<Ipp32fc>::mul(Buffer<Ipp32fc> buff, Ipp32fc c) {
    if (c.re == 1 && c.im == 0) {
		buff.copyTo(*this);
	} else {
		ippsMulC_32fc(buff.ptr, c, ptr, jmin(sz, buff.sz));
	}
	return *this;
}

// template<>
// Buffer<Ipp32f>& Buffer<Ipp32f>::conv(
// 	Buffer<Ipp32f> src1,
// 	Buffer<Ipp32f> src2,
// 	Buffer<Ipp8u> workBuff) {
// 	do {
// 		if (!(sz >= src1.size() + src2.size() - 1)) do {
// 			juce::logAssertion("Buffer. cpp", 692);;
// 			if (juce::juce_isRunningUnderDebugger()) { ::kill(0, 5); };
// 		} while (false);
// 	} while (false);
// 	ippsConvolve_32f(src1.get(), src1.sz, src2.get(), src2.sz, ptr, IppAlgType::ippAlgAuto, workBuff.get());
// 	return *this;
// }