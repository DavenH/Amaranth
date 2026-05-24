#pragma once

#include <App/Transforms.h>
#include <Array/RingBuffer.h>
#include <Array/ScopedAlloc.h>

#include "RealTimePitchTrace.h"
#include "PitchTrackingRequest.h"

using std::pair;
using std::vector;

class RealTimePitchTracker {
public:
    enum Algorithm { AlgoSpectral, AlgoCycleDiff, AlgoYin, AlgoSwipe };

    explicit RealTimePitchTracker(Algorithm algo = AlgoSpectral);
    explicit RealTimePitchTracker(const PitchTrackingRequest& request, Algorithm algo = AlgoSpectral);
    void setSampleRate(int samplerate);
    void write(Buffer<float>& audioBlock);
    void createKernels(double frequencyOfA4);
    int update();
    void setTraceListener(RealTimePitchTraceListener& listener);
    void useDefaultTraceListener();
    Algorithm getAlgorithm() const { return algorithm; }

private:
    int updateSpectral();
    int updatePeriodic();
    int updateSampleTracker(int pitchTrackerAlgorithm);
    void precomputePeriods(double frequencyOfA4, int samplerate);
    PitchTrackingRequest createSampleTrackingRequest() const;

    Algorithm algorithm;
    PitchTrackingRequest request{};
    int sampleRate = 44100;

    vector<Buffer<float>> kernels;
    Transform transform;
    ScopedAlloc<float> kernelMemory;
    ScopedAlloc<float> fftFreqs;
    ScopedAlloc<float> fftMagnitudes;
    ScopedAlloc<float> correlations;

    ScopedAlloc<float> rawBlock;
    ScopedAlloc<float> analysisBlock;
    ScopedAlloc<float> periodScores;
    ScopedAlloc<float> cycleDiffScratch;
    vector<int>        tauTable;

    ReadWriteBuffer rwBuffer;
    ScopedAlloc<float> localBlock;
    ScopedAlloc<float> hannWindow;

    SpinLock bufferLock;
    bool hasWindowedBlock = false;

    int bestKeyIndex = 69;
    RealTimePitchTraceListener defaultTraceListener;
    RealTimePitchTraceListener* traceListener = &defaultTraceListener;

    static constexpr int blockSize = 4096;
};
