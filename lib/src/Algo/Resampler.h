#pragma once


#include "JuceHeader.h"
#include "../Array/ScopedAlloc.h"

class Resampler {
public:
    Resampler();
    ~Resampler();

#ifdef USE_IPP

    void initFixedWithLength(float rollf, float alpha, int inRate,      int outRate, int length,    int bufSize,    int& sourceSize, int& destSize);
    void initFixedWithWindow(float rollf, float alpha, int inRate,      int outRate, float window,  int bufSize,    int& sourceSize, int& destSize);
    void initFixed          (float rollf, float alpha, int inRate,      int outRate, int bufSize,   int& sourceSize, int& destSize);
    void initWithLength     (float rollf, float alpha, double factor,   int length, int nStep, int bufSize, int& sourceSize, int& destSize);
    void initWithWindow     (float rollf, float alpha, double outToInRate, float window, int nStep, int bufSize, int& sourceSize, int& destSize);
    void initWithHistory    (float rollf, float alpha, double outToInRate, int historyLength, int nStep, int bufSize, int& sourceSize, int& destSize);

    void dummyResample(int size);
    void primeWithZeros();
    void reset();

    Buffer<float> finalise();
    Buffer<float> resample(Buffer<float> input);

    [[nodiscard]] int getHistorySize() const { return history; }
    void setRatio(double ratio) { dstToSrc = ratio; }

    void freeFixedState() {
        if (fixedState != nullptr) {
            ippsFree(fixedState);

            fixedState = nullptr;
        }
    }

    void freeState() {
        if (state != nullptr) {
            ippsFree(state);
            state = nullptr;
        }
    }

    static int lengthToHistory(int length)                      { return (((length + 3) & (~3)) >> 1) + 1;                      }
    static float historyToWindow(int history, double rateRatio) { return (float)(2 * (history - 1) * jmin(1.0, rateRatio));     }
    static int windowToHistory(float window, double rateRatio)  { return (int)(window * 0.5 * jmax(1.0, 1.0 / rateRatio)) + 1;  }

private:
    void init(float rollf, float alpha, int& sourceSize, int& destSize);

    /* ----------------------------------------------------------------------------- */

    bool    fixed;          // true for fixed factor filter
    bool    ready;
    bool    haveReset;

    int     length{};       // filter length for resampling with fixed factor (+1 for 0 phase)
    int     nStep{};        // discretization step for filter coefficients
    int     inRate{};       // input frequency
    int     outRate{};      // output frequency
    int     bufSize;        // output frequency
    int     history;        // ~length/2
    int     lastread{};
    int     outLen;

    long    outSamples;

    float   window{};       // size of the ideal lowpass filter window
    float   rollf{};        // roll-off frequency of the filter
    float   alpha{};        // parameter of the Kaiser window

    double dstToSrc;    // resampling factor
    double time;

public:
    Buffer<float> source;
    Buffer<float> dest;

private:
    IppsResamplingPolyphase_32f *state;
    IppsResamplingPolyphaseFixed_32f *fixedState;

    JUCE_LEAK_DETECTOR(Resampler);
#endif

};

