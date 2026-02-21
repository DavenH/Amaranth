#pragma once

#include <App/Transforms.h>
#include <Array/RingBuffer.h>
#include <Array/ScopedAlloc.h>
#include "RealTimePitchTrace.h"
using std::vector;
using std::pair;

class RealTimePitchTracker {
public:
    enum Algorithm { AlgoSpectral, AlgoCycleDiff };

    explicit RealTimePitchTracker(Algorithm algo = AlgoSpectral);
    void setSampleRate(int samplerate);
    void write(Buffer<float>& audioBlock);
    void createKernels(double frequencyOfA4);
    int update();
    void setTraceListener(RealTimePitchTraceListener& listener);
    void useDefaultTraceListener();

private:
    int updateSpectral();
    int updatePeriodic();
    void precomputePeriods(double frequencyOfA4, int samplerate);

    Algorithm algorithm;
    int sampleRate = 44100;

    // ── Spectral (FFT correlation) mode ───────────────────────────────────
    vector<Buffer<float>> kernels;
    Transform transform;
    ScopedAlloc<float> kernelMemory;
    ScopedAlloc<float> fftFreqs;
    ScopedAlloc<float> fftMagnitudes;
    ScopedAlloc<float> correlations;

    // ── CycleDiff (time-domain periodicity) mode ──────────────────────────
    ScopedAlloc<float> rawBlock;         // peak-normalised, no window
    ScopedAlloc<float> periodScores;     // one score per piano key
    ScopedAlloc<float> cycleDiffScratch; // working buffer, length = blockSize
    vector<int>        tauTable;         // integer period per key (samples)

    // ── Shared ────────────────────────────────────────────────────────────
    ReadWriteBuffer rwBuffer;
    ScopedAlloc<float> localBlock;
    ScopedAlloc<float> hannWindow;

    SpinLock bufferLock;
    bool hasWindowedBlock = false;

    int bestKeyIndex = 69;
    RealTimePitchTraceListener defaultTraceListener;
    RealTimePitchTraceListener* traceListener = &defaultTraceListener;

    static constexpr int numKeys   = 88;
    static constexpr int blockSize = 4096;
};
