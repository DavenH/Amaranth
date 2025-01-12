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
    memory.ensureSize(convertToCart ? bufferSize * 2 + 2 : bufferSize + 2);
    fftBuffer = memory.place(bufferSize + 2);

    splitComplex.realp = fftBuffer.get();
    splitComplex.imagp = fftBuffer.get() + (bufferSize / 2);
  #else
    int specSize, specBuffSize, buffSize;
    int ippScaleType = -1;
    switch(scaleType) {
        case DivFwdByN: ippScaleType = IPP_DIV_FWD_BY_N; break;
        case DivInvByN: ippScaleType = IPP_DIV_INV_BY_N; break;
        case NoDivByAny: ippScaleType = IPP_NODIV_BY_ANY; break;
    }

    ippsFFTGetSize_R_32f(order, ippScaleType, ippAlgHintFast, &specSize, &specBuffSize, &buffSize);

    stateBuff.resize(specSize);
    workBuff.resize(buffSize);
    spec = (IppsFFTSpec_R_32f*) stateBuff.get();

    ScopedAlloc<Int8u> initBuff(specBuffSize);
    ippsFFTInit_R_32f(&spec, order, ippScaleType, ippAlgHintFast, stateBuff, initBuff);

    memory.ensureSize(convertToCart ? bufferSize * 2 + 2 : bufferSize + 2);
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
    vDSP_ctoz((DSPComplex*)src.get(), 2, &splitComplex, 1, size/2);
    vDSP_fft_zrip(fftSetup, &splitComplex, 1, order, FFT_FORWARD);
    switch(scaleType) {
        case DivFwdByN:
            fftBuffer.toType<Float32>().mul(1 / size);
            break;
    }

    if (convertToCart) {
        vDSP_zvmags(&splitComplex, 1, magnitudes, 1, size/2);
        vDSP_zvphas(&splitComplex, 1, phases, 1, size/2);
    }
  #else
    ippsFFTFwd_RToCCS_32f(src, fftBuffer, spec, workBuff);
    if(removeOffset) {
        // removes the DC offset term
        // TODO validate that the fft pack in Accelerate is equivalent to CCS
        fftBuffer[0] = 0;
    }
    if (convertToCart) {
        ippsCartToPolar_32fc(reinterpret_cast<Complex32*>(fftBuffer.get()) + 1, magnitudes, phases, size/2);
    }
  #endif
}

void Transform::inverse(const Buffer<Complex32>& fftInput, const Buffer<float>& dest) {
  #ifdef USE_ACCELERATE
    DSPComplex c;
    c.realp = reinterpret_cast<float*>(fftInput.get());
    c.imagp = reinterpret_cast<float*>(fftInput.get()) + 1;
    // this mutates the fftBuffer, referenced by the splitComplex real/imag pointers
    vDSP_ctoz(&c, 1, &splitComplex, 1, size / 2);

    int size = 1 << (order - 1);
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
    int size = 1 << order;

  #ifdef USE_ACCELERATE
    if (convertToCart) {
        vDSP_ztoc(&splitComplex, 1, (DSPComplex*)dest.get(), 2, size/2);
    }

    vDSP_fft_zrip(fftSetup, &splitComplex, 1, order, FFT_INVERSE);
    switch(scaleType) {
        case DivInvByN:
            dest.mul(1.0f / size);
            break;
    }
  #else
    if (convertToCart) {
        ippsPolarToCart_32fc(magnitudes, phases, (Complex32*)fftBuffer.get() + 1, size/2);
    }
    ippsFFTInv_CCSToR_32f(fftBuffer, dest, spec, workBuff);
  #endif
}