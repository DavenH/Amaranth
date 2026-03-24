#include <cmath>
#include "Resampler.h"

#include <Array/VecOps.h>

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
#ifdef USE_IPP
    ,   state      (nullptr)
    ,   fixedState (nullptr)
#endif
{}

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

    bool success = true;

    freeFixedState();

    int stateSize, filtLength, numFilters;
#ifdef USE_IPP
    IppStatus status;
    status      = ippsResamplePolyphaseFixedGetSize_32f(inRate, outRate, length, &stateSize, &filtLength, &numFilters, ippAlgHintFast);
    fixedState  = reinterpret_cast<IppsResamplingPolyphaseFixed_32f *>(ippsMalloc_8u(stateSize));
    status      = ippsResamplePolyphaseFixedInit_32f(inRate, outRate, length, rollf, alpha, fixedState, ippAlgHintFast);
    success     = (status == ippStsNoErr);
#else
    stateSize = filtLength = numFilters = 0;
    ignoreUnused(stateSize, filtLength, numFilters);
    sincInterpolator.reset();
#endif

    sourceSize  = bufSize / dstToSrc + 2 * history + 2;
    destSize    = bufSize + 2 * history + 2;

    ready       = success;
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
    this->rollf = rollf;
    this->alpha = alpha;

    fixed  = false;
    length = (history - 1) << 1;

    freeState();

    int stateSize;
    bool success = true;
#ifdef USE_IPP
    IppStatus status;
    ippsResamplePolyphaseGetSize_32f(window, nStep, &stateSize, ippAlgHintFast);
    state      = reinterpret_cast<IppsResamplingPolyphase_32f*>(ippsMalloc_8u(stateSize));
    status     = ippsResamplePolyphaseInit_32f(window, nStep, rollf, alpha, state, ippAlgHintFast);
    success    = (status == ippStsNoErr);
#else
    stateSize = 0;
    ignoreUnused(stateSize);
    sincInterpolator.reset();
#endif

    sourceSize = bufSize + 2 * history + 2;
    destSize   = bufSize * 2 + 2 * history + 100;

    ready      = success;
}

void Resampler::reset() {
    time      = static_cast<double>(history);
    lastread  = history;
    haveReset = true;

    source.zero(history);
#ifndef USE_IPP
    sincInterpolator.reset();
#endif
}

void Resampler::primeWithZeros() {
    jassert(history <= source.size());

    source.zero(history);
    resample(source.withSize(history));
}

Buffer<float> Resampler::finalise() {
#ifdef USE_IPP
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
#else
    jassert(ready);
    jassert(haveReset);

    source.sectionAtMost(lastread, history).zero();
    lastread += jmin(history, source.size() - lastread);

    outLen = runSincResample(true);
    haveReset = false;

    return dest.withSize(outLen);
#endif
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

#ifdef USE_IPP
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
#else
    ignoreUnused(sourceSize);
    outLen = runSincResample(false);
#endif

    return dest.withSize(outLen);
}

void Resampler::copyStateFrom(const Resampler& other) {
    if (this == &other) {
        return;
    }

    fixed = other.fixed;
    ready = other.ready;
    haveReset = other.haveReset;

    length = other.length;
    nStep = other.nStep;
    inRate = other.inRate;
    outRate = other.outRate;
    bufSize = other.bufSize;
    history = other.history;
    lastread = other.lastread;
    outLen = other.outLen;

    outSamples = other.outSamples;

    window = other.window;
    rollf = other.rollf;
    alpha = other.alpha;

    dstToSrc = other.dstToSrc;
    time = other.time;

    source = other.source;
    dest = other.dest;

#ifdef USE_IPP
    freeState();
    freeFixedState();
#else
    sincInterpolator.reset();
#endif
}

void Resampler::dummyResample(int size) {
#ifdef USE_IPP
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
#else
    const int padding = jmin(size, source.size() - lastread);
    source.offset(lastread).withSize(padding).zero();
    lastread += padding;

    const double ratio = jmax(dstToSrc, 1.0e-9);
    time += size / ratio;

    normalizeSourceWindow();
#endif
}

int Resampler::runLinearResample(bool flushTail) {
    const double ratio = jmax(dstToSrc, 1.0e-9);
    const double sourceAdvance = 1.0 / ratio;

    int produced = 0;
    const int maxOutput = dest.size();

    while (produced < maxOutput) {
        const int index = (int) time;
        const double frac = time - index;

        if (index < 0 || index + 1 >= lastread) {
            break;
        }

        const float a = source[index];
        const float b = source[index + 1];
        dest[produced++] = (1.f - (float) frac) * a + (float) frac * b;

        time += sourceAdvance;
    }

    if (! flushTail) {
        normalizeSourceWindow();
    }

    return produced;
}

int Resampler::runSincResample(bool flushTail) {
#ifdef USE_IPP
    ignoreUnused(flushTail);
    jassertfalse;
    return 0;
#else
    const double ratio = jmax(dstToSrc, 1.0e-9);
    const double sourceToDestRatio = 1.0 / ratio;

    const int keptHistory = flushTail ? 0 : history;
    const int availableInput = jmax(0, lastread - keptHistory);
    const int maxOutputByInput = (int) std::floor(availableInput * ratio);
    const int requestedOutput = jlimit(0, dest.size(), maxOutputByInput);

    if (requestedOutput <= 0) {
        return 0;
    }

    const int consumed = sincInterpolator.process(sourceToDestRatio, source.get(), dest.get(), requestedOutput);
    if (consumed > 0) {
        const int remaining = jmax(0, lastread - consumed);
        if (remaining > 0) {
            VecOps::move(source.offset(consumed).withSize(remaining), source.withSize(remaining));
        }
        lastread = remaining;

        return requestedOutput;
    }

    return runLinearResample(flushTail);
#endif
}

void Resampler::normalizeSourceWindow() {
    const int consumed = jmax(0, (int) time - history);
    if (consumed <= 0) {
        return;
    }

    const int remaining = jmax(0, lastread - consumed);
    if (remaining > 0) {
        VecOps::move(source.offset(consumed).withSize(remaining), source.withSize(remaining));
    }

    lastread = remaining;
    time -= consumed;
}
