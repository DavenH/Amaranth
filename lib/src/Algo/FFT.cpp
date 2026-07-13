#include "FFT.h"

#include "../Util/NumberUtils.h"

#include <algorithm>

Transform::Transform() :
    order(0),
  #ifdef USE_ACCELERATE
    fftSetup(nullptr),
  #else
    spec(nullptr),
  #endif
    convertToCart(false),
    scaleType(DivFwdByN),
    removeOffset(false) {
}

Transform::~Transform() {
    clear();
}

void Transform::clear() {
  #ifdef USE_ACCELERATE
    if (fftSetup != nullptr) {
        vDSP_destroy_fftsetup(fftSetup);
        fftSetup = nullptr;
    }
  #else
    if (spec != nullptr) {
        spec = nullptr;
    }
  #endif
    order = 0;
    fftBuffer.nullify();
    magnitudes.nullify();
    phases.nullify();
}

void Transform::allocate(int bufferSize, ScaleType scaleType, bool convertsToCart) {
    jassert(!(bufferSize & (bufferSize - 1)));

    int newOrder = NumberUtils::log2i(bufferSize);
    if (order == newOrder && scaleType == this->scaleType && convertsToCart == convertToCart) {
        return;
    }

    convertToCart = convertsToCart;
    this->scaleType = scaleType;

    clear();
    ScopedLock sl(lock);
    order = newOrder;

  #ifdef USE_ACCELERATE
    fftSetup = vDSP_create_fftsetup(order, FFT_RADIX2);
    int fftBufferSize = bufferSize + 2; // +2 for dc offset and nyquist bins
    memory.ensureSize(2 * fftBufferSize + (convertToCart ? bufferSize : 0));
    fftBuffer = memory.place(fftBufferSize);
    complex = memory.place(fftBufferSize).toType<Complex32>();

    splitComplex.realp = fftBuffer.get();
    splitComplex.imagp = fftBuffer.get() + bufferSize / 2;
  #else
    int specSize, specBuffSize, buffSize;
    int ippScaleType = -1;
    switch(scaleType) {
        case DivFwdByN:  ippScaleType = IPP_DIV_FWD_BY_N; break;
        case DivInvByN:  ippScaleType = IPP_DIV_INV_BY_N; break;
        case NoDivByAny: ippScaleType = IPP_NODIV_BY_ANY; break;
    }

    ippsFFTGetSize_R_32f(order, ippScaleType, ippAlgHintFast, &specSize, &specBuffSize, &buffSize);

    stateBuff.resize(specSize);
    workBuff.resize(buffSize);
    spec = (IppsFFTSpec_R_32f*) stateBuff.get();

    ScopedAlloc<Int8u> initBuff(specBuffSize);
    ippsFFTInit_R_32f(&spec, order, ippScaleType, ippAlgHintFast, stateBuff, initBuff);

    memory.ensureSize(bufferSize + 2 + (convertToCart ? bufferSize : 0));
    fftBuffer = memory.place(bufferSize + 2);
  #endif

    if (convertToCart) {
        magnitudes = memory.place(bufferSize / 2);
        phases = memory.place(bufferSize / 2);
    }
}

void Transform::forward(Buffer<float> src) {
    if (order == 0) return;

    ScopedLock sl(lock);
    int size = 1 << order;

  #ifdef USE_ACCELERATE
    vDSP_ctoz(reinterpret_cast<DSPComplex *>(src.get()), 2, &splitComplex, 1, size / 2);
    vDSP_fft_zrip(fftSetup, &splitComplex, 1, order, FFT_FORWARD);
    // why does vDSP scale by 2? Nobody knows.
    float multiplier = 0.5;
    if (scaleType == DivFwdByN) {
        multiplier /= static_cast<float>(size);
    }
    fftBuffer.toType<Float32>().mul(multiplier);

    if(removeOffset) {
        // vDSP stores the nyquist bin in im[0] - it's only needed for
        // convolution, which won't be the case if we have `removeOffset` true
        // edit, not sure if this is true anymore.
        // fftBuffer[1] = 0;

        // vDSP stores the DC offset in re[0]
        fftBuffer[0] = 0;
    }

    if (convertToCart) {
        DSPSplitComplex temp;
        int hsize = size / 2;
        int complexBins = hsize - 1;
        temp.realp = fftBuffer.get() + 1;
        temp.imagp = fftBuffer.get() + hsize + 1;
        vDSP_zvmags(&temp, 1, magnitudes, 1, complexBins);
        vvsqrtf(magnitudes.get(), magnitudes.get(), &complexBins);
        vDSP_zvphas(&temp, 1, phases, 1, complexBins);

        float nyquist = fftBuffer[hsize];
        magnitudes[hsize - 1] = nyquist < 0.f ? -nyquist : nyquist;
        phases[hsize - 1] = nyquist < 0.f ? MathConstants<float>::pi : 0.f;
    }
  #else
    ippsFFTFwd_RToCCS_32f(src, fftBuffer, spec, workBuff);

    if(removeOffset) {
        fftBuffer[0] = 0;
    }
    if (convertToCart) {
        int hsize = size / 2;
        int complexBins = hsize - 1;
        ippsCartToPolar_32fc(reinterpret_cast<Complex32*>(fftBuffer.get()+2), magnitudes, phases, complexBins);

        float nyquist = fftBuffer[1];
        magnitudes[hsize - 1] = nyquist < 0.f ? -nyquist : nyquist;
        phases[hsize - 1] = nyquist < 0.f ? MathConstants<float>::pi : 0.f;
    }
  #endif
}

RealFftSpectrum Transform::forwardReal(Buffer<float> src) {
    forward(src);
    return getRealSpectrum();
}

void Transform::inverseReal(RealFftSpectrum fftInput, const Buffer<float>& dest) {
    inverse(fftInput.getStorage(), dest);
}

void Transform::inverse(const Buffer<Complex32>& fftInput, const Buffer<float>& dest) {
  #ifdef USE_ACCELERATE
    int size = 1 << order;
    int packedComplexSize = size / 2;
    jassert(fftInput.size() >= packedComplexSize);

    // this mutates the fftBuffer, referenced by the splitComplex real/imag pointers
    vDSP_ctoz(reinterpret_cast<DSPComplex *>(fftInput.get()), 2, &splitComplex, 1, packedComplexSize);
  #elif defined(USE_IPP)
    Buffer<float> oldBuffer = fftBuffer;
    fftBuffer = fftInput.toType<Ipp32f>();
  #endif
    inverse(dest);
  #ifdef USE_ACCELERATE
  #else
    fftBuffer = oldBuffer;
  #endif
}

void Transform::inverse(Buffer<float> dest) {
    if (order == 0) return;

    ScopedLock sl(lock);

    // this ought to be the length of our real dest buffer
    int size = 1 << order;
    jassert(dest.size() >= size);

  #ifdef USE_ACCELERATE
    if (convertToCart) {
        int hsize = size / 2;
        int complexBins = hsize - 1;
        float* real = fftBuffer.get() + 1;
        float* imag = fftBuffer.get() + hsize + 1;
        vvcosf(real, phases.get(), &complexBins);
        vvsinf(imag, phases.get(), &complexBins);
        vDSP_vmul(real, 1, magnitudes.get(), 1, real, 1, complexBins);
        vDSP_vmul(imag, 1, magnitudes.get(), 1, imag, 1, complexBins);

        float nyquistPhase = phases[hsize - 1];
        float nyquistSign = (nyquistPhase < -MathConstants<float>::halfPi || nyquistPhase > MathConstants<float>::halfPi) ? -1.f : 1.f;
        fftBuffer[hsize] = magnitudes[hsize - 1] * nyquistSign;
    }

    if (removeOffset) {
        fftBuffer[0] = 0;
    }

    vDSP_fft_zrip(fftSetup, &splitComplex, 1, order, FFT_INVERSE);
    // Match the legacy IPP scale policies explicitly after vDSP's unscaled inverse.
    if (scaleType == DivInvByN) {
        fftBuffer.toType<Float32>().mul(1.f / float(size));
    }
    vDSP_ztoc(&splitComplex, 1, (DSPComplex*) dest.get(), 2, size / 2);

  #else
    if (convertToCart) {
        int hsize = size / 2;
        int complexBins = hsize - 1;
        ippsPolarToCart_32fc(magnitudes, phases, (Complex32*)fftBuffer.get() + 1, complexBins);

        float nyquistPhase = phases[hsize - 1];
        float nyquistSign = (nyquistPhase < -MathConstants<float>::halfPi || nyquistPhase > MathConstants<float>::halfPi) ? -1.f : 1.f;
        fftBuffer[1] = magnitudes[hsize - 1] * nyquistSign;
    }
    if (removeOffset) {
        fftBuffer[0] = 0;
    }
    ippsFFTInv_CCSToR_32f(fftBuffer, dest, spec, workBuff);
  #endif
}

int Transform::getFullRealBinCount() const {
    return order == 0 ? 0 : RealFftFullPolarSpectrum::binCountForBufferSize(1 << order);
}

void Transform::copyFullPolarSpectrumTo(Buffer<float> magnitudeDest, Buffer<float> phaseDest) {
    if (order == 0 || magnitudeDest.empty()) {
        return;
    }

    RealFftFullPolarSpectrum::copyFromPacked(
            getRealSpectrum().getEndpoints(),
            magnitudes,
            phases,
            magnitudeDest,
            phaseDest);
}

void Transform::setFullPolarSpectrum(Buffer<float> magnitudeSource, Buffer<float> phaseSource) {
    if (order == 0) {
        return;
    }

    fftBuffer.zero();
    magnitudes.zero();
    phases.zero();

    const RealFftPackedEndpoints endpoints = RealFftFullPolarSpectrum::copyToPacked(
            magnitudeSource,
            phaseSource,
            magnitudes,
            phases);
    setPackedEndpoints(endpoints.dc, endpoints.nyquist);
}

void Transform::setPackedEndpoints(float dc, float nyquist) {
    if (fftBuffer.empty() || order == 0) {
        return;
    }

  #ifdef USE_ACCELERATE
    const int hsize = 1 << (order - 1);
    fftBuffer[0] = dc;
    fftBuffer[hsize] = nyquist;
  #else
    fftBuffer[0] = dc;
    fftBuffer[1] = nyquist;
  #endif
}

void RealFftFullPolarSpectrum::copyFromPacked(
        RealFftPackedEndpoints endpoints,
        Buffer<float> ordinaryMagnitudes,
        Buffer<float> ordinaryPhases,
        Buffer<float> magnitudeDest,
        Buffer<float> phaseDest) {
    if (magnitudeDest.empty()) {
        return;
    }

    const int fullBins = ordinaryMagnitudes.size() + 1;
    const int copyBins = std::min(fullBins, magnitudeDest.size());

    magnitudeDest[0] = magnitudeForSignedEndpoint(endpoints.dc);
    if (!phaseDest.empty()) {
        phaseDest[0] = phaseForSignedEndpoint(endpoints.dc);
    }

    if (copyBins <= 1) {
        return;
    }

    const int bodyBins = std::min(copyBins - 1, ordinaryMagnitudes.size());
    ordinaryMagnitudes.withSize(bodyBins).copyTo({
            magnitudeDest.get() + 1,
            bodyBins
    });

    if (!phaseDest.empty()) {
        const int phaseBins = std::min({ copyBins - 1, ordinaryPhases.size(), phaseDest.size() - 1 });
        ordinaryPhases.withSize(phaseBins).copyTo({
                phaseDest.get() + 1,
                phaseBins
        });
    }
}

RealFftPackedEndpoints RealFftFullPolarSpectrum::copyToPacked(
        Buffer<float> magnitudeSource,
        Buffer<float> phaseSource,
        Buffer<float> ordinaryMagnitudes,
        Buffer<float> ordinaryPhases) {
    RealFftPackedEndpoints endpoints;

    if (magnitudeSource.empty()) {
        return endpoints;
    }

    const float dcPhase = !phaseSource.empty() ? phaseSource[0] : 0.f;
    endpoints.dc = signedEndpoint(magnitudeSource[0], dcPhase);

    const int nyquistIndex = ordinaryMagnitudes.size();
    if (magnitudeSource.size() > nyquistIndex) {
        const float nyquistPhase = phaseSource.size() > nyquistIndex ? phaseSource[nyquistIndex] : 0.f;
        endpoints.nyquist = signedEndpoint(magnitudeSource[nyquistIndex], nyquistPhase);
    }

    if (magnitudeSource.size() <= 1) {
        return endpoints;
    }

    const int bodyBins = std::min(magnitudeSource.size() - 1, ordinaryMagnitudes.size());
    Buffer<float>(
            magnitudeSource.get() + 1,
            bodyBins).copyTo(ordinaryMagnitudes.withSize(bodyBins));

    if (!phaseSource.empty()) {
        const int phaseBins = std::min({ phaseSource.size() - 1, ordinaryPhases.size(), bodyBins });
        Buffer<float>(
                phaseSource.get() + 1,
                phaseBins).copyTo(ordinaryPhases.withSize(phaseBins));
    }

    return endpoints;
}

float RealFftFullPolarSpectrum::magnitudeForSignedEndpoint(float value) {
    return value < 0.f ? -value : value;
}

float RealFftFullPolarSpectrum::phaseForSignedEndpoint(float value) {
    return value < 0.f ? MathConstants<float>::pi : 0.f;
}

float RealFftFullPolarSpectrum::signedEndpoint(float magnitude, float phase) {
    return magnitude * (phase < -MathConstants<float>::halfPi || phase > MathConstants<float>::halfPi ? -1.f : 1.f);
}
