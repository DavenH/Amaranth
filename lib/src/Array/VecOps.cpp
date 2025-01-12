#include "VecOps.h"
#include "ArrayDefs.h"
#include "ScopedAlloc.h"
int globalVecOpsSizeErrorCount = 0;

#define ERROR_COUNTER globalVecOpsSizeErrorCount

#ifdef USE_ACCELERATE
#define VIMAGE_H
#include <Accelerate/Accelerate.h>

#define FN_ARG_PATTERN   src1.get(), 1, src2.get(), 1, dst.get(), 1, vDSP_Length(dst.size())
#define MOVE_ARG_PATTERN src.get(), dst.get(), 1, src.size(), 1, 1
#define CPLX_ARG_PATTERN &srcA, 2, &srcB, 2, &dest, 2, vDSP_Length(dst.size())

#define declareForF32_F64_Cplx(op, conjArg) \
    template<> void VecOps::op(SRCA_SRCB_DST(Float32)) { BUFFS_DST_CHECK vDSP_v##op(FN_ARG_PATTERN); } \
    template<> void VecOps::op(SRCA_SRCB_DST(Float64)) { BUFFS_DST_CHECK vDSP_v##op##D(FN_ARG_PATTERN); } \
    template<> void VecOps::op(SRCA_SRCB_DST(Complex32)) { CPLX_TRIADIC_SETUP(src1, src2, dst); vDSP_zv##op(CPLX_ARG_PATTERN conjArg); }

// a add b -> c
// a sub b -> c
// a mul b -> c
// a div b -> c
defineAddSubMulDiv(declareForF32_F64_Cplx)

#define declareForF32_F64(op, fn) \
    template<> void VecOps::op(SRC_DST(Float32)) { BUFFS_EQ_CHECK vDSP_##fn(MOVE_ARG_PATTERN); } \
    template<> void VecOps::op(SRC_DST(Float64)) { BUFFS_EQ_CHECK vDSP_##fn##D(MOVE_ARG_PATTERN); }

declareForF32_F64(move, mmov)

template<> void VecOps::roundDown(Buffer<Float32> src, Buffer<Int8s> dst)  { vDSP_vfix8(src, 1, reinterpret_cast<char*>(dst.get()), 1, src.size()); }
template<> void VecOps::roundDown(Buffer<Float32> src, Buffer<Int16s> dst) { vDSP_vfix16(src, 1, dst, 1, src.size()); }
template<> void VecOps::roundDown(Buffer<Float32> src, Buffer<Int32s> dst) { vDSP_vfix32(src, 1, dst, 1, src.size()); }
template<> void VecOps::roundDown(Buffer<Float64> src, Buffer<Int16s> dst) { vDSP_vfix16D(src, 1, dst, 1, src.size()); }

template<>
void VecOps::mul(Buffer<Float32> src, Float32 k, Buffer<Float32> dst) {
    BUFFS_EQ_CHECK
    vDSP_vsmul(src.get(), 1, &k, dst.get(), 1, src.size());
}
template<> void VecOps::mul(Float32* src, Float32 k, Float32* dst, int len) {
    vDSP_vsmul(src, 1, &k, dst, 1, len);
}
template<>
void VecOps::mul(Buffer<Float64> src, Float64 k, Buffer<Float64> dst) {
    BUFFS_EQ_CHECK
    vDSP_vsmulD(src.get(), 1, &k, dst.get(), 1, src.size());
}
template<>
void VecOps::mul(Buffer<Complex32> src, Complex32 val, Buffer<Complex32> dst) {
    BUFFS_EQ_CHECK
    DSPSplitComplex d, s, k;
    k.realp = reinterpret_cast<float*>(&val);
    k.imagp = reinterpret_cast<float*>(&val) + 1;
    d.realp = reinterpret_cast<float*>(dst.get());
    d.imagp = d.realp + 1;
    s.realp = reinterpret_cast<float*>(src.get());
    s.imagp = s.realp + 1;
    vDSP_zvzsml(&d, 1, &k, &s, 1, src.size());
}
template<> void VecOps::addProd(Float32* src, Float32 k, Float32* dst, int len) {
    vDSP_vsma(src, 1, &k, dst, 1, dst, 1, len);
}

#define defineDiff(type, fn) \
    template<> void VecOps::diff(Buffer<type> src, Buffer<type> dst) { \
        if(dst.size() < 2) return;   \
        vDSP_##fn(src.get(), 1,      \
                  src.get() + 1, 1,  \
                  dst.get(), 1,      \
                  vDSP_Length(jmin(src.size() - 1, dst.size() - 1))); \
        dst[dst.size() - 1] = dst[dst.size() - 2]; \
    }

defineDiff(Float32, vsub)
defineDiff(Float64, vsubD)

template<> void VecOps::interleave(Buffer<Float32> x, Buffer<Float32> y, Buffer<Float32> dst) {
    if(x.empty()) return;
    jassert(dst.size() >= 2 * x.size());
    DSPSplitComplex z;
    z.realp = x.get();
    z.imagp = y.get();
    auto* xp = reinterpret_cast<DSPComplex*>(dst.get());
    vDSP_ztoc(&z, 1, xp, 1, x.size());
}

template<> void VecOps::interleave(Buffer<Float64> x, Buffer<Float64> y, Buffer<Float64> dst) {
    if(x.empty()) return;
    jassert(dst.size() >= 2 * x.size());
    DSPDoubleSplitComplex z;
    z.realp = x.get();
    z.imagp = y.get();
    auto* xp = reinterpret_cast<DSPDoubleComplex*>(dst.get());
    vDSP_ztocD(&z, 1, xp, 1, x.size());
}

#define defineAllocate(T) template<> T* VecOps::allocate<T>(int size) { return (T*) malloc(size * sizeof(T)); }
#define defineDeallocate(T) template<> void VecOps::deallocate<T>(T* ptr) { free(ptr); }
defineForAllTypes(defineAllocate)
defineForAllTypes(defineDeallocate)

template<> void VecOps::convert(Buffer<Float64> src, Buffer<Float32> dst) {
    vDSP_vdpsp(src.get(), 1, dst.get(), 1, src.size());
}
template<> void VecOps::convert(Buffer<Float32> src, Buffer<Float64> dst) {
    vDSP_vspdp(src.get(), 1, dst.get(), 1, src.size());
}

template<> void VecOps::zero(Float32* src, int size) { vDSP_vclr(src, 1, size); }
template<> void VecOps::zero(Float64* src, int size) { vDSP_vclrD(src, 1, size); }

#define defineCopy(T) \
template<> void VecOps::copy(const T* src, T* dst, int size) { \
    memcpy(dst, src, size * sizeof(T));  \
}

defineForAllTypes(defineCopy)

template<>
void VecOps::conv(Buffer<Float32> src1, Buffer<Float32> src2, Buffer<Float32> dst) {
    vDSP_conv(
        src1.get(), 1,
        src2.get(), 1,
        dst.get(), 1,
        vDSP_Length(jmin(dst.size(), src1.size() + src2.size() - 1)),
        vDSP_Length(src2.size())
    );
}


#elif defined(USE_IPP)
//#else

template<> void VecOps::zero(Float32* src, int size) { ippsZero_32f(src, size); }
template<> void VecOps::zero(Float64* src, int size) { ippsZero_64f(src, size); }

#define FN_ARG_PATTERN src2.get(), src1.get(), dst.get(), jmin(src1.size(), src2.size(), dst.size())
#define MOVE_ARG_PATTERN src.get(), dst.get(), jmin(src.size(), dst.size())

#define declareForF32_F64_Cplx(op, fn) \
    template<> void VecOps::op(SRCA_SRCB_DST(Float32))   { ipps##fn##_32f(FN_ARG_PATTERN); }  \
    template<> void VecOps::op(SRCA_SRCB_DST(Float64))   { ipps##fn##_64f(FN_ARG_PATTERN); }  \
    template<> void VecOps::op(SRCA_SRCB_DST(Complex32)) { ipps##fn##_32fc(FN_ARG_PATTERN); }

declareForF32_F64_Cplx(add, Add)
declareForF32_F64_Cplx(sub, Sub)
declareForF32_F64_Cplx(mul, Mul)
declareForF32_F64_Cplx(div, Div)

#define declareForF32_F64(op, fn) \
    template<> void VecOps::op(SRC_DST(Float32)) { ipps##fn##_32f(MOVE_ARG_PATTERN); } \
    template<> void VecOps::op(SRC_DST(Float64)) { ipps##fn##_64f(MOVE_ARG_PATTERN); }

declareForF32_F64(move, Move)

template<> void VecOps::roundDown(Buffer<Float32> src, Buffer<Int8u> dst)  { ippsConvert_32f8u_Sfs (src, dst, src.size(), ippRndZero, 0); }
template<> void VecOps::roundDown(Buffer<Float32> src, Buffer<Int16s> dst) { ippsConvert_32f16s_Sfs(src, dst, src.size(), ippRndZero, 0); }
template<> void VecOps::roundDown(Buffer<Float32> src, Buffer<Int32s> dst) { ippsConvert_32f32s_Sfs(src, dst, src.size(), ippRndZero, 0); }
template<> void VecOps::roundDown(Buffer<Float64> src, Buffer<Int16s> dst) { ippsConvert_64f16s_Sfs(src, dst, src.size(), ippRndZero, 0); }

#define defineCopy(T, S) template<> void VecOps::copy(const T* src, T* dst, int size) { \
    ippsCopy_##S(src, dst, size);  \
}
defineForAllIppTypes(defineCopy);

template<> void VecOps::mul(Buffer<Float32> src, Float32 k, Buffer<Float32> dst) {
    BUFFS_EQ_CHECK
    ippsMulC_32f(src.get(), k, dst.get(), src.size());
}
template<> void VecOps::mul(Buffer<Float64> src, Float64 k, Buffer<Float64> dst) {
    BUFFS_EQ_CHECK
    ippsMulC_64f(src.get(), k, dst.get(), src.size());
}
template<> void VecOps::mul(Float32* src, Float32 k, Float32* dst, int len) {
    ippsMulC_32f(src, k, dst, len);
}
template<> void VecOps::addProd(Float32* src, Float32 k, Float32* dst, int len) {
    ippsAddProductC_32f(src, k, dst, len);
}

#define defineDiff(type, fn) \
    template<> void VecOps::diff(Buffer<type> src, Buffer<type> dst) { \
        if(dst.size() < src.size() - 1 || dst.size() < 2) return; \
        fn(src.get(), src.get() + 1, dst.get(), src.size() - 1); \
        dst[dst.size() - 1] = dst[dst.size() - 2]; \
    }

defineDiff(Float32, ippsSub_32f)
defineDiff(Float64, ippsSub_64f)

template<> void VecOps::interleave(Buffer<Float32> x, Buffer<Float32> y, Buffer<Float32> dst) {
    if(x.empty()) return;
    jassert(dst.size() >= 2 * x.size());
    ippsRealToCplx_32f(x.get(), y.get(), (Complex32*) dst.get(), x.size());
}

template<> void VecOps::interleave(Buffer<Float64> x, Buffer<Float64> y, Buffer<Float64> dst) {
    if(x.empty()) return;
    jassert(dst.size() >= 2 * x.size());
    ippsRealToCplx_64f(x.get(), y.get(), (Complex64*) dst.get(), x.size());
}

#define defineAllocate(T, S) template<> T* VecOps::allocate<T>(int size) { return ippsMalloc_##S(size); }
defineForAllIppTypes(defineAllocate)

#define defineDeallocate(T) template<> void VecOps::deallocate<T>(T* ptr) { ippsFree(ptr); }
defineForAllTypes(defineDeallocate)

template<> void VecOps::convert(Buffer<Float64> src, Buffer<Float32> dst) {
    ippsConvert_64f32f(src.get(), dst.get(), src.size());
}
template<> void VecOps::convert(Buffer<Float32> src, Buffer<Float64> dst) {
    ippsConvert_32f64f(src.get(), dst.get(), src.size());
}

template<> void VecOps::conv(Buffer<Float32> src1, Buffer<Float32> src2, Buffer<Float32> dst) {
    jassert(dst.size() >= src1.size() + src2.size() - 1);
    int buffSize;
    ippsConvolveGetBufferSize(src1.size(), src2.size(), IppDataType::ipp32f, ippAlgAuto, &buffSize);
    ScopedAlloc<Ipp8u> workBuff(buffSize);
    jassert(dst.size() >= src1.size() + src2.size() - 1);
    ippsConvolve_32f(src1.get(), src1.size(), src2.get(), src2.size(), dst.get(), ippAlgAuto, workBuff);
}

/*
template<> void VecOps::fir(const Buffer<Float32>& src, Buffer<Float32> dst, float relFreq, bool trim) {
    int specSize, buffSize, kernelBuffSize;
    const int kernelSize = 32;
    ippsFIRSRGetSize(kernelSize, ipp32f, &specSize, &buffSize);
    ippsFIRGenGetBufferSize(kernelSize, &kernelBuffSize);
    ScopedAlloc<Int8u> kernelWorkBuff(kernelBuffSize);

    ScopedAlloc<Ipp64f> taps64(kernelSize);
    ScopedAlloc<Ipp32f> taps32(kernelSize);
    ippsFIRGenLowpass_64f(relFreq, taps64, kernelSize, ippWinBlackman, ippTrue, kernelWorkBuff);
    ippsConvert_64f32f(taps64, taps32, kernelSize);

    ScopedAlloc<Ipp8u> stateBuff(specSize);
    ScopedAlloc<Ipp8u> workBuff(buffSize);
    auto* state = (IppsFIRSpec_32f*) stateBuff.get();
    ippsFIRSRInit_32f(taps32, kernelSize, ippAlgAuto, state);
    ippsFIRSR_32f(src, dst, src.size(), state, taps32, taps32, workBuff);
}
*/

#endif

template<> void VecOps::divCRev(Buffer<Float32> src, Float32 k, Buffer<Float32> dst) { src.copyTo(dst); dst.divCRev(k); }
// template<> void VecOps::divCRev(Buffer<Float64> src, Float64 k, Buffer<Float64> dst) { src.copyTo(dst); dst.divCRev(k); }
template<> void VecOps::flip(Buffer<Float32> src, Buffer<Float32> dst) { src.copyTo(dst); dst.flip();  }
template<> void VecOps::flip(Buffer<Float64> src, Buffer<Float64> dst) { src.copyTo(dst); dst.flip();  }

template<> void VecOps::sinc(Buffer<Float32> kernel, Buffer<Float32> window, float relFreq) {
    if (kernel.size() <= 1) { return; }
    kernel.ramp(0, relFreq * 2 * M_PI / kernel.size());
    window.blackman();
    kernel.mul(window);
}

template<> void VecOps::fir(const Buffer<Float32>& src, Buffer<Float32> dst, float relFreq, bool trim) {
    ScopedAlloc<Float32> mem(64);
    Buffer<Float32> kernel = mem.place(mem.size() / 2);
    Buffer<Float32> window = mem.place(mem.size() / 2);
    sinc(kernel, window, relFreq);
    jassert(dst.size() >= trim ? src.size() : src.size() + 32);
    conv(
        src.withSize(trim ? src.size() - kernel.size() : src.size()),
        kernel,
        dst
    );
}