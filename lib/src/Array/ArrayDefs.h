#pragma once

#define EMPTY_CHECK if(sz == 0) return *this;
#define BUFF_CHECK if(buff.size() == 0) return *this;
#define SIZE_CHECK if(size == 0) return *this;
#define UNO_ARG_CHECK if(c == 1) return *this;
#define NULL_ARG_CHECK if(c == 0) return *this;
#define EMPTY_CHECK_ZERO if(sz == 0) return 0;
#define BUFFS_CHECK if(src1.size() < sz || src2.size() < sz) { ++ERROR_COUNTER; return *this; }
#define BUFFS_DST_CHECK if(src1.size() < dst.size() || src2.size() < dst.size()) { ++ERROR_COUNTER; return; }
#define BUFFS_EQ_CHECK if(src.size() != dst.size()) { ++ERROR_COUNTER; return; }
#define SRCA_SRCB_DST(T) Buffer<T> src1, Buffer<T> src2, Buffer<T> dst
#define SRC_DST(T) Buffer<T> src, Buffer<T> dst

#define defineForAllTypes(T) \
    T(Int8u)      \
    T(Int16s)     \
    T(Int32s)     \
    T(Float32)    \
    T(Float64)    \
    T(Complex32)  \
    T(Complex64)

#define defineForAllIppTypes(T) \
    T(Int8u, 8u)      \
    T(Int16s, 16s)    \
    T(Int32s, 32s)    \
    T(Float32, 32f)   \
    T(Float64, 64f)   \
    T(Complex32, 32fc)\
    T(Complex64, 64fc)

#define defineAudioBufferConstructor(T) \
    template<> Buffer<T>::Buffer(AudioBuffer<T>& audioBuffer, int chan) : \
        ptr(audioBuffer.getWritePointer(chan)), \
        sz(audioBuffer.getNumSamples()) {}


#ifdef USE_ACCELERATE

#define CPLX_TRIADIC_SETUP(src1, src2, dst)            \
    DSPSplitComplex dest, srcA, srcB;                  \
    dest.realp = reinterpret_cast<float*>(dst.get());  \
    dest.imagp = dest.realp + 1;                       \
    srcA.realp = reinterpret_cast<float*>(src1.get()); \
    srcA.imagp = srcA.realp + 1;                       \
    srcB.realp = reinterpret_cast<float*>(src2.get()); \
    srcB.imagp = srcB.realp + 1;

#define CPLX_DIADIC_SETUP                              \
    DSPSplitComplex dest, src;                         \
    dest.realp = reinterpret_cast<float*>(ptr);        \
    dest.imagp = dest.realp + 1;                       \
    src.realp = reinterpret_cast<float*>(buff.get());  \
    src.imagp = src.realp + 1;
#endif

// vDSP has an annoying asymmetry with the multiplication op, which takes an extra argument
#define mulConjArg , 1
#define defineAddSubMulDiv(T) \
  T(add, ) \
  T(sub, ) \
  T(mul, mulConjArg) \
  T(div, )

#ifdef USE_IPP
#endif