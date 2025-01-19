#include "FFT.h"

#include "../Util/NumberUtils.h"

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
    convertToCart = convertsToCart;
    jassert(!(bufferSize & (bufferSize - 1)));

    int newOrder = NumberUtils::log2i(bufferSize);
    if (order == newOrder && scaleType == this->scaleType) {
        return;
    }
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
        temp.realp = fftBuffer.get() + 1;
        temp.imagp = fftBuffer.get() + hsize + 1;
        vDSP_zvmags(&temp, 1, magnitudes, 1, hsize);
        vvsqrtf(magnitudes.get(), magnitudes.get(), &hsize);
        vDSP_zvphas(&temp, 1, phases, 1, hsize);
    }
  #else
    ippsFFTFwd_RToCCS_32f(src, fftBuffer, spec, workBuff);

    if(removeOffset) {
        fftBuffer[0] = 0;
    }
    if (convertToCart) {
        ippsCartToPolar_32fc(reinterpret_cast<Complex32*>(fftBuffer.get()) + 1, magnitudes, phases, size/2);
    }
  #endif
}

void Transform::inverse(const Buffer<Complex32>& fftInput, const Buffer<float>& dest) {
  #ifdef USE_ACCELERATE
    int size = 1 << order;
    // this mutates the fftBuffer, referenced by the splitComplex real/imag pointers
    vDSP_ctoz(reinterpret_cast<DSPComplex *>(fftInput.get()), 1, &splitComplex, 1, size / 2);

    jassert(fftInput.size() == size + 1);
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
        float* real = fftBuffer.get() + 1;
        float* imag = fftBuffer.get() + hsize + 1;
        vvcosf(real, phases.get(), &hsize);
        vvsinf(imag, phases.get(), &hsize);
        vDSP_vmul(real, 1, magnitudes.get(), 1, real, 1, hsize);
        vDSP_vmul(imag, 1, magnitudes.get(), 1, imag, 1, hsize);
    }

    vDSP_fft_zrip(fftSetup, &splitComplex, 1, order, FFT_INVERSE);
    // vDSP implicitly div's the output by N
    if (scaleType == NoDivByAny) {
        fftBuffer.toType<Float32>().mul(size);
    }
    vDSP_ztoc(&splitComplex, 1, (DSPComplex*) dest.get(), 2, size / 2);

  #else
    if (convertToCart) {
        ippsPolarToCart_32fc(magnitudes, phases, (Complex32*)fftBuffer.get() + 1, size/2);
    }
    ippsFFTInv_CCSToR_32f(fftBuffer, dest, spec, workBuff);
  #endif
}