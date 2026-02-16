#pragma once

#include <App/Transforms.h>
#include <Array/RingBuffer.h>
#include <Array/ScopedAlloc.h>
#include "RealTimePitchTrace.h"
using std::vector;
using std::pair;

class RealTimePitchTracker {
public:
    RealTimePitchTracker();
    void setSampleRate(int samplerate);
    void write(Buffer<float>& audioBlock);
    void createKernels(double frequencyOfA4);
    int update();
    void setTraceListener(RealTimePitchTraceListener& listener);
    void useDefaultTraceListener();

private:
    vector<Buffer<float>> kernels;
    ReadWriteBuffer rwBuffer;
    Transform transform;

    ScopedAlloc<float> kernelMemory;
    ScopedAlloc<float> fftFreqs;
    ScopedAlloc<float> localBlock;
    ScopedAlloc<float> fftMagnitudes;
    ScopedAlloc<float> correlations;
    ScopedAlloc<float> hannWindow;

    SpinLock bufferLock;
    bool hasWindowedBlock = false;

    int bestKeyIndex = 69;
    RealTimePitchTraceListener defaultTraceListener;
    RealTimePitchTraceListener* traceListener = &defaultTraceListener;

    static constexpr int numKeys = 88;
    static constexpr int blockSize = 4096;
};
