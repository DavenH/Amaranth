#include "VecOps.h"
#include "ArrayDefs.h"
int globalVecOpsSizeErrorCount = 0;

#define ERROR_COUNTER globalVecOpsSizeErrorCount

#ifdef USE_ACCELERATE
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

template<> void VecOps::roundDown(Buffer<Float32> src, Buffer<Int16s> dst) { vDSP_vfix16(src, 1, dst, 1, src.size()); }
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


//#define defineInterleave(type, fn) \

template<> void VecOps::interleave(Buffer<Float32> x, Buffer<Float32> y, Buffer<Float32> dst) {
    if(x.empty()) return;
    jassert(dst.size() >= 2 * x.size());
    DSPSplitComplex z;
    z.realp = reinterpret_cast<Float32*>(x.get());
    z.imagp = reinterpret_cast<Float32*>(y.get());
    DSPComplex* xp = reinterpret_cast<DSPComplex*>(dst.get());
    vDSP_ztoc(&z, 1, xp, 1, x.size());
}

template<> void VecOps::interleave(Buffer<Float64> x, Buffer<Float64> y, Buffer<Float64> dst) {
    if(x.empty()) return;
    jassert(dst.size() >= 2 * x.size());
    DSPDoubleSplitComplex z;
    z.realp = reinterpret_cast<Float64*>(x.get());
    z.imagp = reinterpret_cast<Float64*>(y.get());
    DSPDoubleComplex* xp = reinterpret_cast<DSPDoubleComplex*>(dst.get());
    vDSP_ztocD(&z, 1, xp, 1, x.size());
}

#define defineAllocate(T) template<> T* VecOps::allocate<T>(int size) { return (T*) malloc(size * sizeof(T)); }
#define defineDeallocate(T) template<> void VecOps::deallocate<T>(T* ptr) { delete ptr; }
declareForAllTypes(defineAllocate)
declareForAllTypes(defineDeallocate)


template<> void VecOps::convert(Buffer<Float64> src, Buffer<Float32> dst) {
    vDSP_vdpsp(src.get(), 1, dst.get(), 1, src.size());
}
template<> void VecOps::convert(Buffer<Float32> src, Buffer<Float64> dst) {
    vDSP_vspdp(src.get(), 1, dst.get(), 1, src.size());
}


#elif defined(USE_IPP)
//#else

#define FN_ARG_PATTERN src1.get(), src2.get(), dst.get(), dst.size()
#define MOVE_ARG_PATTERN src.get(), dst.get(), dst.get(), src.size()

#define declareForF32_F64_Cplx(op, fn) \
    template<> void VecOps::op(SRCA_SRCB_DST(Float32))   { BUFFS_DST_CHECK ipps##fn##_32f(FN_ARG_PATTERN); }  \
    template<> void VecOps::op(SRCA_SRCB_DST(Float64))   { BUFFS_DST_CHECK ipps##fn##_64f(FN_ARG_PATTERN); }  \
    template<> void VecOps::op(SRCA_SRCB_DST(Complex32)) { BUFFS_DST_CHECK ipps##fn##_32fc(FN_ARG_PATTERN); }

declareForF32_F64_Cplx(add, Add)
declareForF32_F64_Cplx(sub, Sub)
declareForF32_F64_Cplx(mul, Mul)
declareForF32_F64_Cplx(div, Div)

#define declareForF32_F64(op, fn) \
    template<> void VecOps::op(SRC_DST(Float32)) { BUFFS_EQ_CHECK ipps_##fn##_32f(MOVE_ARG_PATTERN); } \
    template<> void VecOps::op(SRC_DST(Float64)) { BUFFS_EQ_CHECK ipps_##fn##_64f(MOVE_ARG_PATTERN); }

declareForF32_F64(move, Move)

template<> void VecOps::roundDown(Buffer<Float32> src, Buffer<Int16s> dst) { ippsConvert_32f16s_Sfs(src, dst, src.size(), ippRndZero, 0); }
template<> void VecOps::roundDown(Buffer<Float64> src, Buffer<Int16s> dst) { ippsConvert_64f16s_Sfs(src, dst, src.size(), ippRndZero, 0); }

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
        if(dst.size() < 2) return; \
        BUFFS_EQ_CHECK \
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

template<> Int8u* VecOps::allocate<Int8u>(int size) { return ippsMalloc_8u(size); }
template<> Int8s* VecOps::allocate<Int8s>(int size) { return ippsMalloc_8s(size); }
template<> Int16s* VecOps::allocate<Int16s>(int size) { return ippsMalloc_16s(size); }
template<> Int32s* VecOps::allocate<Int32s>(int size) { return ippsMalloc_32s(size); }
template<> Float32* VecOps::allocate<Float32>(int size) { return ippsMalloc_32f(size); }
template<> Float64* VecOps::allocate<Float64>(int size) { return ippsMalloc_64f(size); }
template<> Complex32* VecOps::allocate<Complex32>(int size) { return ippsMalloc_32fc(size); }
template<> Complex64* VecOps::allocate<Complex64>(int size) { return ippsMalloc_64fc(size); }

#define defineDeallocate(T) template<> void VecOps::deallocate<T>(T* ptr) { ippsFree(ptr); }
declareForAllTypes(defineDeallocate)

template<> void VecOps::convert(Buffer<Float64> src, Buffer<Float32> dst) {
    ippsConvert_64f32f(src.get(), dst.get(), src.size());
}
template<> void VecOps::convert(Buffer<Float32> src, Buffer<Float64> dst) {
    ippsConvert_32f64f(src.get(), dst.get(), src.size());
}

#endif

template<> void VecOps::divCRev(Buffer<Float32> src, Float32 k, Buffer<Float32> dst) { dst.divCRev(k); }
template<> void VecOps::divCRev(Buffer<Float64> src, Float64 k, Buffer<Float64> dst) { dst.divCRev(k); }
template<> void VecOps::flip(Buffer<Float32> src, Buffer<Float32> dst) { src.copyTo(dst); dst.flip();  }
template<> void VecOps::flip(Buffer<Float64> src, Buffer<Float64> dst) { src.copyTo(dst); dst.flip();  }

