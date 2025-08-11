#pragma once

#include <App/Transforms.h>
#include <Array/RingBuffer.h>
#include <Array/ScopedAlloc.h>
using std::vector;
using std::pair;

class RealTimePitchTracker {
public:
    RealTimePitchTracker();
    void setSampleRate(int samplerate);
    void write(Buffer<float>& audioBlock);
    void createKernels(double frequencyOfA4);
    pair<int, float> update();

private:
    vector<Buffer<float>> kernels;
    ReadWriteBuffer rwBuffer;
    Transform transform;

    ScopedAlloc<float> kernelMemory;
    ScopedAlloc<float> fftFreqs;
    ScopedAlloc<float> localBlock;
    ScopedAlloc<float> fftMagnitudes;
    ScopedAlloc<float> hannWindow;

    std::atomic<bool> pendingPitchUpdate{false};

    SpinLock bufferLock;

    float bestPitch = 440.0f;
    int bestKeyIndex = 48;

    static constexpr int numKeys = 88;
    static constexpr int blockSize = 4096;
};
