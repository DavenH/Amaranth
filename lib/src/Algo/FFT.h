#pragma once

#include "../Array/ScopedAlloc.h"
#include "../Array/Buffer.h"

#ifdef USE_ACCELERATE
  #define VIMAGE_H
  #include <Accelerate/Accelerate.h>
#else
#include <ipp.h>
#endif

class Transform {
public:
    enum ScaleType {
        NoDivByAny, DivFwdByN, DivInvByN
    };

    Transform();
    virtual ~Transform();

    /**
      * @param bufferSize the size of the buffer we expect to call
      *     with the forward() pass. Power of 2. If already called, is a no-op.
      * @param scaleType how to scale the forward or inverse pass
      * @param convertsToCart whether we plan to convert complex outputs to
      *     power and phase spectra. If so we allocate more working memory for
      *     these buffers.
      */
    void allocate(int bufferSize, ScaleType scaleType = DivFwdByN, bool convertsToCart = false);

    /*
     * Resets memory buffers
     */
    void clear();

    /*
     * Performs the forward FFT transform. Scales the results according to the scaleType:
     * - NoDivByAny => output is unscaled
     * - DivFwdByN  => output is scaled by 1 / bufferSize
     * - DivInvByN  => N/A
     */
    void forward(Buffer<float> src);

    /*
     * Performs the inverse FFT transform. Scales the results according to the scaleType:
     * - NoDivByAny => output is unscaled
     * - DivFwdByN  => N/A
     * - DivInvByN  => output is scaled by 1 / bufferSize
     */
    void inverse(Buffer<float> dst);

    /*
     * Perform the inverse FFT transform upon the provided complex input and store the real output in the dest buffer
     */
    void inverse(const Buffer<Complex32>& fftInput, const Buffer<float>& dest);

    // void setComplex(Buffer<Complex32> buffer);

    /*
     * Re-
     */
    void scaleTypeChanged();

    /*
     * Set whether to remove DC offset
     */
    void setRemovesOffset(bool does) { removeOffset = does; }

    Buffer<Complex32> getComplex() const {
        int size = 1 << (order - 1);
        jassert(fftBuffer.size() >= size * 2 + 2);

        return fftBuffer.toType<Complex32>();
    }

    /*
     * Returns the power spectrum of the FFT data
     */
    Buffer<float> getMagnitudes() { return magnitudes; }
    /*
     * Returns the phase spectrum of the FFT data
     */
    Buffer<float> getPhases() { return phases; }

    Transform& operator<<(const Buffer<float>& buffer) {
        forward(buffer);
        return *this;
    }

    Transform& operator>>(const Buffer<float>& buffer) {
        inverse(buffer);
        return *this;
    }

private:
    bool convertToCart, removeOffset;
    ScaleType scaleType;
    int order;

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
