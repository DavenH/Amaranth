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

void Transform::allocate(int bufferSize, bool convertsToCart) {
    convertToCart = convertsToCart;
    jassert(!(bufferSize & (bufferSize - 1)));

    int newOrder = NumberUtils::log2i(bufferSize);
    if (order == newOrder) {
        return;
    }

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
    ippsFFTGetSize_R_32f(order, scaleType, ippAlgHintFast, &specSize, &specBuffSize, &buffSize);

    stateBuff.resize(specSize);
    workBuff.resize(buffSize);
    spec = (IppsFFTSpec_R_32f*) stateBuff.get();

    ScopedAlloc<Ipp8u> initBuff(specBuffSize);
    ippsFFTInit_R_32f(&spec, order, scaleType, ippAlgHintFast, stateBuff, initBuff);

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

    if (convertToCart) {
        vDSP_zvmags(&splitComplex, 1, magnitudes, 1, size/2);
        vDSP_zvphas(&splitComplex, 1, phases, 1, size/2);
    }
  #else
    ippsFFTFwd_RToCCS_32f(src, fftBuffer, spec, workBuff);

    if (convertToCart) {
        ippsCartToPolar_32fc(reinterpret_cast<Ipp32fc*>(fftBuffer.get()) + 1, magnitudes, phases, size/2);
    }
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
    float scale = 1.0f / size;
    vDSP_vsmul(dest, 1, &scale, dest, 1, size);
  #else
    if (convertToCart) {
        ippsPolarToCart_32fc(magnitudes, phases, (Ipp32fc*)fftBuffer.get() + 1, size/2);
    }
    ippsFFTInv_CCSToR_32f(fftBuffer, dest, spec, workBuff);
  #endif
}