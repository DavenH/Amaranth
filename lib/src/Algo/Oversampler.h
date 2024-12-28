#pragma once

#include <ipp.h>
#include "../Array/ScopedAlloc.h"
#include "../Array/Buffer.h"

class SingletonRepo;

class Oversampler {
public:
    explicit Oversampler(SingletonRepo* repo, int kernelSize = 32);
    virtual ~Oversampler();

    void resetDelayLine();
    void sampleDown(Buffer<float> src, Buffer<float> dest, bool wrapTail = false);
    void setKernelSize(int size);
    void setOversampleFactor(int factor);
    void start(Buffer<float>& buffer);
    void stop();

    int getLatencySamples();
    Buffer<float> getMemoryBuffer(int size);
    Buffer<float> getTail();

    void setMemoryBuf(const Buffer<float>& buffer) { memoryBuf = buffer; }
    int getOversampleFactor() const { return oversampleFactor; }

private:
    void updateTaps();

    int phase, oversampleFactor;

    ScopedAlloc<Ipp8u> stateUpBuf, stateDownBuf;
    ScopedAlloc<Ipp8u> workBuffUp, workBuffDown;
    ScopedAlloc<Ipp32f> firTaps, firUpDly, firDownDly;

    Buffer<float> memoryBuf, audioBuf;
    Buffer<float>* oversampBuf;

    IppsFIRSpec_32f *filterUpState, *filterDownState;

    JUCE_LEAK_DETECTOR(Oversampler);
};
