#include "FFT.h"
#include "../Util/NumberUtils.h"

Transform::Transform() :
        order		(0)
    , 	spec		(nullptr)
    ,	convertToCart(false)
    ,	scaleType	(IPP_FFT_DIV_FWD_BY_N)
    ,	removeOffset(false) {
}

Transform::~Transform() {
    clear();
}

void Transform::clear() {
    if (spec != nullptr) {
        ScopedLock sl(lock);

        spec 		= nullptr;
        order 		= 0;

        fftBuffer	.nullify();
        magnitudes	.nullify();
        phases		.nullify();
    }
}

void Transform::allocate(int bufferSize, bool convertsToCart) {
    convertToCart = convertsToCart;

    jassert(! (bufferSize & (bufferSize - 1)));

    int newOrder = NumberUtils::log2i(bufferSize);
    if(order == newOrder) {
        return;
    }

    clear();

    ScopedLock sl(lock);

    order = newOrder;

    int specSize, specBuffSize, buffSize;
    ippsFFTGetSize_R_32f(order, scaleType, ippAlgHintFast, &specSize, &specBuffSize, &buffSize);

    stateBuff.resize(specSize);
    workBuff.resize(buffSize);
    spec = (IppsFFTSpec_R_32f*) stateBuff.get();


    // TODO can I actually free this, use spec after??
    ScopedAlloc<Ipp8u> initBuff(specBuffSize);

    ippsFFTInit_R_32f(&spec, order, scaleType, ippAlgHintFast, stateBuff, initBuff);

    memory.ensureSize(convertToCart ? bufferSize * 2 + 2 : bufferSize + 2);
    fftBuffer = memory.place(bufferSize + 2);

    if (convertToCart) {
        magnitudes = memory.place(bufferSize / 2);
        phases = memory.place(bufferSize / 2);
    }
}

void Transform::forward(Buffer<float> src) {
    if(spec == nullptr) {
        return;
    }

    jassert(order > 0);
    jassert(src.size() >= 1 << order);

    ScopedLock sl(lock);

    ippsFFTFwd_RToCCS_32f(src, fftBuffer, spec, workBuff);

    if(convertToCart) {
        ippsCartToPolar_32fc(reinterpret_cast<Ipp32fc*>(fftBuffer.get()) + 1, magnitudes, phases, 1 << (order - 1));
    }
}

void Transform::inverse(Buffer<float> dest) {
    if(spec == nullptr) {
        return;
    }

    ScopedLock sl(lock);

    jassert(dest.size() >= 1 << order);

    if(convertToCart) {
        ippsPolarToCart_32fc(magnitudes, phases, (Ipp32fc*)fftBuffer.get() + 1, 1 << (order - 1));
    }

    ippsFFTInv_CCSToR_32f(fftBuffer, dest, spec, workBuff);
}

void Transform::inverse(const Buffer<Ipp32fc>& fftInput, const Buffer<float>& dest) {
    Buffer<float> oldBuffer = fftBuffer;

    int size = 1 << (order - 1);
    jassert(fftInput.size() == size + 1);

    fftBuffer = fftInput.toType<Ipp32f>();
    inverse(dest);
    fftBuffer = oldBuffer;
}

Buffer<Ipp32fc> Transform::getComplex() const {
    int size = 1 << (order - 1);
    jassert(fftBuffer.size() >= size * 2 + 2);

    return fftBuffer.toType<Ipp32fc>();
}

