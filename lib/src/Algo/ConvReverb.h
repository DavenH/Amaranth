#pragma once
#include <vector>
#include "FFT.h"
#include "../App/SingletonAccessor.h"
#include "../Array/StereoBuffer.h"
#include "../Util/MicroTimer.h"

using std::vector;

class BlockConvolver {
public:
    BlockConvolver();
    void init(int sizeOfBlock, int kernelSize, float decay);
    void init(int sizeOfBlock, const Buffer<float>& kernel);
    void process(const Buffer<float>& input, Buffer<float> output);
    void reset();

    int getBlockSize() const { return blockSize; }

private:
    void init(int sizeOfBlock, const Buffer<float>& kernel, bool useNoise);

    static bool isProbablyEmpty(const Buffer<Complex32>& buffer) {
        if(buffer.empty())
            return true;

        bool isEmpty = true;
        int size = buffer.size() / 2;

        while (isEmpty && size > 0) {
            const Complex32& val = buffer[size];
            isEmpty &= (val == Complex32());
            size >>= 1;
        }

        return isEmpty;
    }

    bool useNoise, active;
    int blockSize, numBlocks, currSegment, inputBufferPos;
    unsigned seed;
    float noiseDecay;

    Random random;
    Transform fft;

    ScopedAlloc<float> memory;
    ScopedAlloc<Complex32> cplxMemory;

    Buffer<float>       fftBuffer, overlapBuffer, inputBuffer, decayLevels;
    Buffer<Complex32>   sumBuffer, mulBuffer, convBuffer, noiseBuffer;

    vector<Buffer<Complex32>> inputBlocks, kernelBlocks;

    Complex32 baseNoiseLevel;

    friend class ConvReverb;
};

class ConvReverb :
        public SingletonAccessor {
public:
    int tailInputPos, precalcPos;
    int headBlockSize, tailBlockSize;

    explicit ConvReverb(SingletonRepo* repo);

    void init(int headSize, int tailSize, const Buffer<float>& kernel);
    void process(const Buffer<float>& input, Buffer<float> output);
    static void basicConvolve(Buffer<float> input, Buffer<float> response, Buffer<float> output);
    void reset() override;
    void test();
    void test(int inputSize, int irSize, int headSize, int tailSize, int bufferSize, bool refCheck, bool useTwoStage);
    Buffer<float> convolve(const Buffer<float>& inputFrq, Buffer<float> kernelFrq);

private:
    int headClocks, tailClocksA, tailClocks;

    float wetLevel, dryLevel;

    BlockConvolver  headConvolver, neckConvolver, tailConvolver;
    StereoBuffer    outBuffer;
    MicroTimer      timer;

    Buffer<float>   kernel;
    Buffer<float>   tailOutput, tailPrecalc;
    Buffer<float>   neckOutput, neckPrecalc;
    Buffer<float>   neckInput,  tailInput;

    ScopedAlloc<float> memory;
};