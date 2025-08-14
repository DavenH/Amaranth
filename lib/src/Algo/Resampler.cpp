#include "Resampler.h"

#include <Array/VecOps.h>
#include <cmath>

#ifdef USE_IPP

const float resamplingScale = 0.98f;

Resampler::Resampler() :
        ready      (false)
    ,   haveReset  (false)
    ,   fixed      (false)
    ,   dstToSrc   (1.)
    ,   time       (0)
    ,   outLen     (0)
    ,   bufSize    (0)
    ,   history    (5)
    ,   outSamples (0)
    ,   state      (nullptr)
    ,   fixedState (nullptr) {
}

Resampler::~Resampler() {
   ready = false;

   freeFixedState();
   freeState();
}

void Resampler::initFixedWithLength(float rollf, float alpha,
                                    int inRate,
                                    int outRate,
                                    int length,
                                    int bufSize,
                                    int& sourceSize, int& destSize) {
    fixed    = true;
    history  = lengthToHistory(length);   // for time rounding
    dstToSrc = (double)outRate / (double)inRate;
    window   = historyToWindow(history, dstToSrc);

    initFixed(rollf, alpha, inRate, outRate, bufSize, sourceSize, destSize);
}

void Resampler::initFixedWithWindow(
        float rollf, float alpha,
        int inRate, int outRate,
        float window, int bufSize,
        int& sourceSize, int& destSize) {
    dstToSrc     = (double)outRate / (double)inRate;
    history     = windowToHistory(window, dstToSrc);
    this->window = window;

    initFixed(rollf, alpha, inRate, outRate, bufSize, sourceSize, destSize);
}

void Resampler::initFixed(float rollf, float alpha, int inRate,
        int outRate, int bufSize, int& sourceSize, int& destSize) {
    this->rollf   = rollf;
    this->alpha   = alpha;
    this->inRate  = inRate;
    this->outRate = outRate;
    this->bufSize = bufSize;
    this->nStep   = -1;

    fixed         = true;
    length        = (history - 1) << 1;

    IppStatus status;

    freeFixedState();

    int stateSize, filtLength, numFilters;
    status      = ippsResamplePolyphaseFixedGetSize_32f(inRate, outRate, length, &stateSize, &filtLength, &numFilters, ippAlgHintFast);
    fixedState  = reinterpret_cast<IppsResamplingPolyphaseFixed_32f *>(ippsMalloc_8u(stateSize));
    status      = ippsResamplePolyphaseFixedInit_32f(inRate, outRate, length, rollf, alpha, fixedState, ippAlgHintFast);

    sourceSize  = bufSize / dstToSrc + 2 * history + 2;
    destSize    = bufSize + 2 * history + 2;

    ready       = (status == ippStsNoErr);
}

void Resampler::initWithLength(
        float rollf,
        float alpha,
        double outToIn,
        int length,
        int nStep,
        int bufSize,
        int& sourceSize,
        int& destSize) {
    history = lengthToHistory(length);   // for time rounding
    window  = historyToWindow(history, outToIn);

    this->dstToSrc = outToIn;
    this->nStep    = nStep;
    this->bufSize  = bufSize;

    init(rollf, alpha, sourceSize, destSize);
}

void Resampler::initWithWindow(
        float rollf,
        float alpha,
        double outputRateToInputRateRatio,
        float window,
        int nStep,
        int bufSize,
        int& sourceSize,
        int& destSize) {
    history       = windowToHistory(window, outputRateToInputRateRatio);

    this->dstToSrc = outputRateToInputRateRatio;
    this->window   = window;
    this->nStep    = nStep;
    this->bufSize  = bufSize;

    init(rollf, alpha, sourceSize, destSize);
}

void Resampler::initWithHistory(
        float rollf,
        float alpha,
        double outputRateToInputRateRatio,
        int historyLength,
        int nStep,
        int bufSize,
        int& sourceSize,
        int& destSize) {

    history = historyLength;
    window  = historyToWindow(history, outputRateToInputRateRatio);

    this->dstToSrc = outputRateToInputRateRatio;
    this->nStep    = nStep;
    this->bufSize  = bufSize;

    init(rollf, alpha, sourceSize, destSize);
}

void Resampler::init(float rollf, float alpha, int& sourceSize, int& destSize) {
    IppStatus status;

    this->rollf = rollf;
    this->alpha = alpha;

    fixed  = false;
    length = (history - 1) << 1;

    freeState();

    int stateSize;
    ippsResamplePolyphaseGetSize_32f(window, nStep, &stateSize, ippAlgHintFast);
    state      = reinterpret_cast<IppsResamplingPolyphase_32f*>(ippsMalloc_8u(stateSize));
    status     = ippsResamplePolyphaseInit_32f(window, nStep, rollf, alpha, state, ippAlgHintFast);

    sourceSize = bufSize + 2 * history + 2;
    destSize   = bufSize * 2 + 2 * history + 100;

    ready      = (status == ippStsNoErr);
}

void Resampler::reset() {
    time      = static_cast<double>(history);
    lastread  = history;
    haveReset = true;

    source.zero(history);
}

void Resampler::primeWithZeros() {
    jassert(history <= source.size());

    source.zero(history);
    resample(source.withSize(history));
}

Buffer<float> Resampler::finalise() {
    source.sectionAtMost(lastread, history).zero();
    int sourceSize = lastread - (int) time;

    if(fixed) {
        ippsResamplePolyphaseFixed_32f(source, sourceSize, dest, resamplingScale, &time, &outLen, fixedState);
    } else {
        ippsResamplePolyphase_32f(source, sourceSize, dest, dstToSrc, resamplingScale, &time, &outLen, state);
    }

    haveReset = false;

    if (sourceSize == 0) {
        outLen = 0;
    }

    return dest.withSize(outLen);
}

Buffer<float> Resampler::resample(Buffer<float> input) {
    jassert(ready);
    jassert(haveReset);
    jassert(! fixed || history >= windowToHistory(window, dstToSrc));

    input.copyTo(source + lastread);
    lastread += input.size();

    jassert(lastread <= source.size());

    if(lastread > source.size()) {
        return {dest, 0};
    }

    int sourceSize = lastread - history - (int) time;

    if(fixed) {
        ippsResamplePolyphaseFixed_32f(source, sourceSize, dest, resamplingScale, &time, &outLen, fixedState);
    } else {
        ippsResamplePolyphase_32f(source, sourceSize, dest, dstToSrc, resamplingScale, &time, &outLen, state);
    }
    int toMove = lastread + history - (int)time;
    VecOps::move(
        source.offset((int)time - history).withSize(toMove),
        source.withSize(toMove)
    );

    lastread -= (int)time - history;
    time -= (int)time - history;

    if(sourceSize == 0) {
        outLen = 0;
    }

    return dest.withSize(outLen);
}

void Resampler::dummyResample(int size) {
    source.offset(lastread).zero(bufSize - lastread);

    lastread += size;
    time += size / dstToSrc;

    int toMove = lastread + history - (int)time;
    VecOps::move(
        source.offset((int)time - history).withSize(toMove),
        source.withSize(toMove)
    );

    lastread -= (int)time - history;
    time -= (int)time - history;
}

#endif
