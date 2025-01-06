#pragma once

#include "../Array/ScopedAlloc.h"
#include "../Array/Buffer.h"

#ifdef USE_ACCELERATE
  #include <Accelerate/Accelerate.h>
#else
  #include <ipp.h>
#endif

class Transform {
public:
    enum ScaleType {
      NoDivByAny,
      DivFwdByN,
      DivInvByN
    };

    Transform();
    virtual ~Transform();

    void allocate(int size, bool convertsToCart = false);
    void clear();
    void forward(Buffer<float> src);
    void inverse(Buffer<float> dst);
    void inverse(const Buffer<Complex32>& fftInput, const Buffer<float>& dest);
    void setComplex(Buffer<Complex32> buffer);

    void setFFTScaleType(int type)      { scaleType = type;     }
    void setRemovesOffset(bool does)    { removeOffset = does;  }

    Buffer<Complex32> getComplex() const;
    Buffer<float> getMagnitudes()       { return magnitudes;    }
    Buffer<float> getPhases()           { return phases;        }
    Buffer<float> getFFTBuffer()        { return fftBuffer;     }

    Transform& operator<<(const Buffer<float>& buffer) { forward(buffer);   return *this; }
    Transform& operator>>(const Buffer<float>& buffer) { inverse(buffer);   return *this; }

private:
    bool convertToCart, removeOffset;
    int scaleType, order;

    CriticalSection lock;
    ScopedAlloc<float> memory;
    Buffer<float> fftBuffer, magnitudes, phases;
    ScopedAlloc<Int8u> workBuff;

  #ifdef USE_ACCELERATE
    FFTSetup fftSetup;
    DSPSplitComplex splitComplex;
  #else
    ScopedAlloc<Int8u> stateBuff;
    IppsFFTSpec_R_32f* spec;
  #endif
};
