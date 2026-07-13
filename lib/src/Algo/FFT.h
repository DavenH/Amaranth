#pragma once

#include "../Array/ScopedAlloc.h"
#include "../Array/Buffer.h"

#ifdef USE_ACCELERATE
  #define VIMAGE_H
  #include <Accelerate/Accelerate.h>
#else
#include <ipp.h>
#endif

class ComplexSpectrum {
public:
    ComplexSpectrum() = default;
    explicit ComplexSpectrum(Buffer<Complex32> storage) : storage(storage) {}

    [[nodiscard]] bool empty() const { return storage.empty(); }
    [[nodiscard]] int size() const { return storage.size(); }

    Buffer<Complex32> getStorage() const { return storage; }

private:
    Buffer<Complex32> storage;
};

struct RealFftPackedEndpoints {
    float dc {};
    float nyquist {};
};

class RealFftSpectrum {
public:
    RealFftSpectrum() : packedSize(0) {}
    explicit RealFftSpectrum(Buffer<Complex32> storage) :
            storage     (storage)
        ,   packedSize  (storage.size()) {}
    RealFftSpectrum(Buffer<Complex32> storage, int packedSize) :
            storage     (storage)
        ,   packedSize  (packedSize) {
        jassert(packedSize <= storage.size());
    }

    [[nodiscard]] bool empty() const { return storage.empty(); }
    [[nodiscard]] int size() const { return packedSize; }

    float getDC() const { return realPart(storage[0]); }
    float getNyquist() const { return imagPart(storage[0]); }
    RealFftPackedEndpoints getEndpoints() const { return { getDC(), getNyquist() }; }
    Complex32 getBin(int index) const { return storage[index]; }
    Buffer<Complex32> getStorage() const { return storage; }

    Buffer<Complex32> getOrdinaryComplexBins() const {
        return packedSize > 1 ? Buffer<Complex32>(storage.get() + 1, packedSize - 1) : Buffer<Complex32>();
    }

    void setDC(float dc) { setPackedEndpoint(dc, getNyquist()); }
    void setNyquist(float nyquist) { setPackedEndpoint(getDC(), nyquist); }

    void zero() { storage.zero(); }

    void copyTo(RealFftSpectrum dest) const {
        storage.copyTo(dest.getStorage());
    }

    void addProduct(RealFftSpectrum left, RealFftSpectrum right) {
        if (storage.empty()) {
            return;
        }

        setPackedEndpoint(
            getDC() + left.getDC() * right.getDC(),
            getNyquist() + left.getNyquist() * right.getNyquist()
        );

        getOrdinaryComplexBins().addProduct(
            left.getOrdinaryComplexBins(),
            right.getOrdinaryComplexBins()
        );
    }

private:
    static float realPart(const Complex32& value) {
        return perfSplit(value.re, value.real());
    }

    static float imagPart(const Complex32& value) {
        return perfSplit(value.im, value.imag());
    }

    void setPackedEndpoint(float dc, float nyquist) {
        perfSplit(
            storage[0].re = dc; storage[0].im = nyquist,
            storage[0].real(dc); storage[0].imag(nyquist)
        );
    }

    Buffer<Complex32> storage;
    int packedSize;
};

class RealFftFullPolarSpectrum {
public:
    static int binCountForBufferSize(int bufferSize) { return bufferSize / 2 + 1; }

    static void copyFromPacked(
            RealFftPackedEndpoints endpoints,
            Buffer<float> ordinaryMagnitudes,
            Buffer<float> ordinaryPhases,
            Buffer<float> magnitudeDest,
            Buffer<float> phaseDest);

    static RealFftPackedEndpoints copyToPacked(
            Buffer<float> magnitudeSource,
            Buffer<float> phaseSource,
            Buffer<float> ordinaryMagnitudes,
            Buffer<float> ordinaryPhases);

private:
    static float magnitudeForSignedEndpoint(float value);
    static float phaseForSignedEndpoint(float value);
    static float signedEndpoint(float magnitude, float phase);
};

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
    RealFftSpectrum forwardReal(Buffer<float> src);

    /*
     * Performs the inverse FFT transform. Scales the results according to the scaleType:
     * - NoDivByAny => output is unscaled
     * - DivFwdByN  => N/A
     * - DivInvByN  => output is scaled by 1 / bufferSize
     */
    void inverse(Buffer<float> dst);
    void inverseReal(RealFftSpectrum fftInput, const Buffer<float>& dest);

    /*
     * Perform the inverse FFT transform upon the provided complex input and store the real output in the dest buffer
     */
    void inverse(const Buffer<Complex32>& fftInput, const Buffer<float>& dest);

    /*
     * Set whether to remove DC offset
     */
    void setRemovesOffset(bool does) { removeOffset = does; }

    Buffer<Complex32> getComplex() const {
        return getRealSpectrum().getStorage();
    }

    RealFftSpectrum getRealSpectrum() const {
        int size = 1 << (order - 1);
      #ifdef USE_ACCELERATE
        jassert(complex.size() >= size);
        vDSP_ztoc(&splitComplex, 1, (DSPComplex*) complex.get(), 2, size);
        return RealFftSpectrum(complex.withSize(size));
      #else
        jassert(fftBuffer.size() >= size * 2 + 2);

        return RealFftSpectrum(fftBuffer.toType<Complex32>(), size);
      #endif
    }

    /*
     * Returns the power spectrum of the FFT data
     */
    Buffer<float> getMagnitudes() { return magnitudes; }
    /*
     * Returns the phase spectrum of the FFT data
     */
    Buffer<float> getPhases() { return phases; }

    int getFullRealBinCount() const;
    void copyFullPolarSpectrumTo(Buffer<float> magnitudeDest, Buffer<float> phaseDest);
    void setFullPolarSpectrum(Buffer<float> magnitudeSource, Buffer<float> phaseSource);

    Transform& operator<<(const Buffer<float>& buffer) {
        forward(buffer);
        return *this;
    }

    Transform& operator>>(const Buffer<float>& buffer) {
        inverse(buffer);
        return *this;
    }

private:
    void setPackedEndpoints(float dc, float nyquist);

    bool convertToCart, removeOffset;
    ScaleType scaleType;
    int order;

    CriticalSection lock;
    ScopedAlloc<float> memory;
    Buffer<float> fftBuffer, magnitudes, phases;
    ScopedAlloc<Int8u> workBuff;

  #ifdef USE_ACCELERATE
    FFTSetup fftSetup;
    Buffer<Complex32> complex;
    DSPSplitComplex splitComplex;
  #else
    ScopedAlloc<Int8u> stateBuff;
    IppsFFTSpec_R_32f* spec;
  #endif
};
