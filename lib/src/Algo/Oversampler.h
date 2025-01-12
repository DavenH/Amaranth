#pragma once

#include "../Array/ScopedAlloc.h"
#include "../Array/Buffer.h"

class SingletonRepo;

class Oversampler {
public:
    explicit Oversampler(SingletonRepo* repo, int kernelSize = 32);
    virtual ~Oversampler();

    void resetDelayLine();
    void setKernelSize(int size);
    void setOversampleFactor(int factor);

    /**
     * Samples down by `oversampleFactor`, first filtering at the Nyquist frequency.
     */
    void sampleDown(Buffer<float> src, Buffer<float> dest, bool wrapTail = false);

    /**
     * Starts oversampling, reassigning the `buffer` argument to
     * the oversample buffer iff the oversample factor is not 1.
     *
     * Usage:
     * <code>
     * Buffer<float> audio(data, size);
     * oversampler.startOversamplingBlock(audio);
     * audio.tanh(); // audio.size() is now `size * oversampleFactor`
     * oversampler.stopOversamplingBlock();
     * // audio.size() is now `size`
     * </code>
     * audio has now been tanh'd without aliasing distortion
     */
    void startOversamplingBlock(Buffer<float>& buffer);

    /**
     * Ends oversampling, by filtering at the Nyquist frequency
     * and downsampling the internal `oversampBuf`.
     *
     * Clients can now continue processing the buffer passed into start().
     */
    void stopOversamplingBlock();

    int getLatencySamples();

    Buffer<float> getMemoryBuffer(int size);
    Buffer<float> getTail();

    void setMemoryBuf(const Buffer<float>& buffer) { memoryBuf = buffer; }
    int getOversampleFactor() const { return oversampleFactor; }

private:
    void updateTaps();

    int phase, oversampleFactor;

    ScopedAlloc<Int8u> stateUpBuf, stateDownBuf;
    ScopedAlloc<Int8u> workBuffUp, workBuffDown;
    ScopedAlloc<Float32> firTaps, firUpDly, firDownDly;

    Buffer<float> memoryBuf, audioBuf;
    Buffer<float>* oversampBuf;

#ifdef USE_IPP
    IppsFIRSpec_32f *filterUpState, *filterDownState;
#endif

    JUCE_LEAK_DETECTOR(Oversampler);
};
