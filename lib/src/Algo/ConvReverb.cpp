#include "ConvReverb.h"
#include "FFT.h"
#include "../Array/VecOps.h"
#include "../App/MemoryPool.h"
#include "../App/SingletonRepo.h"
#include "../App/Transforms.h"
#include "../Util/Arithmetic.h"
#include "../Definitions.h"

ConvReverb::ConvReverb(SingletonRepo* repo) :
        SingletonAccessor(repo, "ConvReverb")
    ,   outBuffer(2)
    ,   wetLevel(0.5f)
    ,   dryLevel(1.f) {
}

void ConvReverb::reset() {
    headBlockSize   = 0;
    tailBlockSize   = 0;
    precalcPos      = 0;
    tailInputPos    = 0;

    headConvolver.reset();
    tailConvolver.reset();
    neckConvolver.reset();

    memory.clear();
}

void ConvReverb::init(int headSize, int tailSize, const Buffer<float>& kernel) {
    reset();

    if(headSize == 0 || tailSize == 0) {
        return;
    }

    headSize = jmax(1, headSize);

    if(headSize > tailSize) {
        std::swap(headSize, tailSize);
    }

    headBlockSize = NumberUtils::nextPower2(headSize);
    tailBlockSize = NumberUtils::nextPower2(tailSize);

    // test after setting vars
    if(kernel.empty()) {
        return;
    }

    while (tailBlockSize > kernel.size() / 2) {
        tailBlockSize /= 2;
    }

    memory.ensureSize(6 * tailBlockSize);

    ///
    int headLenIR = jmin(kernel.size(), tailBlockSize);
    headConvolver.init(headBlockSize, kernel.withSize(headLenIR));

    if(kernel.size() > tailBlockSize) {
        int neckIrSize = jmin(kernel.size() - tailBlockSize, tailBlockSize);
        neckConvolver.init(tailBlockSize / 2, kernel.section(tailBlockSize, neckIrSize));

        neckOutput  = memory.place(tailBlockSize);
        neckPrecalc = memory.place(tailBlockSize);
    } else {
        neckConvolver.active = false;
        neckOutput.nullify();
        neckPrecalc.nullify();
    }

    if (kernel.size() > 2 * tailBlockSize) {
        int tailIrSize = kernel.size() - 2 * tailBlockSize;
        tailConvolver.init(tailBlockSize, kernel.section(2 * tailBlockSize, tailIrSize));

        tailOutput  = memory.place(tailBlockSize);
        tailPrecalc = memory.place(tailBlockSize);
        tailInput   = memory.place(tailBlockSize);
    } else {
        tailConvolver.active = false;
        tailOutput  .nullify();
        tailPrecalc .nullify();
        tailInput   .nullify();
    }

    if(!neckPrecalc.empty() || !tailPrecalc.empty())
        neckInput = memory.place(tailBlockSize);

    tailOutput  .zero();
    tailPrecalc .zero();
    tailInput   .zero();
    neckOutput  .zero();
    neckInput   .zero();
    neckPrecalc .zero();

    tailInputPos    = 0;
    precalcPos      = 0;
}

void ConvReverb::process(
        const Buffer<float>& input,
        Buffer<float> output) {

    output.zero();

    headConvolver.process(input, output);

    if(neckInput.empty()) {
        return;
    }
    int samplesDone = 0;

    while (samplesDone < input.size()) {
        int remaining = input.size() - samplesDone;
        int samplesToDo = jmin(remaining, headBlockSize - (tailInputPos % headBlockSize));

        jassert(tailInputPos + samplesToDo <= tailBlockSize);

        {
            // sum first tail block
            if(neckConvolver.active) {
                Buffer<float> neck = neckPrecalc.section(precalcPos, samplesToDo);

                if(! neck.isProbablyEmpty())
                    output.section(samplesDone, samplesToDo).add(neck);
            }

            // sum 2nd - nth tail block
            if (tailConvolver.active) {
                Buffer<float> tail = tailPrecalc.section(precalcPos, samplesToDo);

                if (!tail.isProbablyEmpty()) {
                    output.section(samplesDone, samplesToDo).add(tail);
                }
            }

            precalcPos += samplesToDo;
        }

        // fill input buffer for tail convolution
        input.section(samplesDone, samplesToDo).copyTo(neckInput + tailInputPos);
        tailInputPos += samplesToDo;

        jassert(tailInputPos <= tailBlockSize);

        // convolution: first tail block
        if (neckConvolver.active && tailInputPos % neckConvolver.getBlockSize() == 0) {
            jassert(tailInputPos >= headBlockSize);

            int blockOffset = tailInputPos - neckConvolver.getBlockSize();
            neckConvolver.process(neckInput .section(blockOffset, neckConvolver.getBlockSize()),
                                  neckOutput.section(blockOffset, neckConvolver.getBlockSize()));

            if (tailInputPos == tailBlockSize) {
                std::swap(neckPrecalc, neckOutput);
            }
        }

        // convolution: 2nd - nth tail block
        if (tailConvolver.active && tailInputPos == tailBlockSize) {
            std::swap(tailPrecalc, tailOutput);

            neckInput.copyTo(tailInput);
            tailConvolver.process(tailInput, tailOutput);
        }

        if(tailInputPos == tailBlockSize) {
            tailInputPos = 0;
            precalcPos = 0;
        }

        samplesDone += samplesToDo;
    }
}

void BlockConvolver::reset() {
    blockSize       = 0;
    numBlocks       = 0;
    currSegment     = 0;
    active          = true;
    useNoise        = false;

    memory.clear();
    cplxMemory.clear();

    inputBlocks.clear();
    kernelBlocks.clear();
}

void BlockConvolver::init(int sizeOfBlock, const Buffer<float>& kernel) {
    init(sizeOfBlock, kernel, false);
}

void BlockConvolver::init(int sizeOfBlock, int kernelSize, float decay) {
    init(sizeOfBlock, Buffer<float>(nullptr, kernelSize), true);
    baseNoiseLevel = {0.f, 1.f};
    noiseDecay = decay;
}

BlockConvolver::BlockConvolver() :
        useNoise(false)
    ,   active(true)
    ,   numBlocks(0)
    ,   currSegment(0)
    ,   inputBufferPos(0)
    ,   seed(0)
    ,   blockSize(0)
    ,   baseNoiseLevel{}
    ,   noiseDecay(0) {
}

void BlockConvolver::init(int sizeOfBlock, const Buffer<float>& kernel, bool usesNoise) {
    reset();

    sizeOfBlock     = NumberUtils::nextPower2(sizeOfBlock);
    useNoise        = usesNoise;
    blockSize       = sizeOfBlock;
    numBlocks       = static_cast<int>(::ceilf(kernel.size() / float(blockSize)));
    inputBufferPos  = 0;
    currSegment     = 0;

    int segmentSize = 2 * blockSize;
    int complexSize = blockSize + 1; // in Complex32 units (8 bytes)

    fft.allocate(segmentSize, Transform::ScaleType::DivInvByN);

    int complexReserve = complexSize * (numBlocks * 2 + 3);

    cplxMemory  .resize(complexReserve);
    memory      .resize(blockSize * 2 + segmentSize + (useNoise ? numBlocks : 0));
    cplxMemory  .zero();
    memory      .zero();

    sumBuffer     = cplxMemory.place(complexSize);
    convBuffer    = cplxMemory.place(complexSize);

    overlapBuffer = memory.place(blockSize);
    inputBuffer   = memory.place(blockSize);
    fftBuffer     = memory.place(segmentSize);

    kernelBlocks.clear();
    inputBlocks.clear();

    for (int i = 0; i < numBlocks; ++i) {
        Buffer<Complex32> segment = cplxMemory.place(complexSize);

        int remaining = kernel.size() - (i * blockSize);
        int fftLen = (remaining >= blockSize) ? blockSize : remaining;

        Buffer<Complex32> irSegment = cplxMemory.place(complexSize);

        kernel.section(i * blockSize, fftLen).copyTo(fftBuffer);

        fftBuffer.offset(fftLen).zero();
        fft.forward(fftBuffer);
        fft.getComplex().copyTo(irSegment);

        kernelBlocks.push_back(irSegment);
        inputBlocks.push_back(segment);
    }
}

void BlockConvolver::process(const Buffer<float>& input, Buffer<float> output) {
    if (numBlocks == 0) {
        output.zero();
        return;
    }

    int samplesProcessed = 0;

    while (samplesProcessed < input.size()) {
        bool inputWasEmpty   = inputBufferPos == 0;
        int samplesToProcess = jmin(input.size() - samplesProcessed, blockSize - inputBufferPos);

        input.section(samplesProcessed, samplesToProcess).copyTo(inputBuffer + inputBufferPos);
        
        inputBuffer.copyTo(fftBuffer);
        fftBuffer.offset(blockSize).zero();
        fft.forward(fftBuffer);
        fft.getComplex().copyTo(inputBlocks[currSegment]);

        if (inputWasEmpty) {
            sumBuffer.zero();

            for (int i = 1; i < numBlocks; ++i) {
                int kernIndex   = i;
                int inputIndex  = (currSegment + i) % numBlocks;

                sumBuffer.addProduct(kernelBlocks[kernIndex], inputBlocks[inputIndex]);
            }
        }

        sumBuffer.copyTo(convBuffer);
        convBuffer.addProduct(kernelBlocks[0], inputBlocks[currSegment]);
        
        fft.inverse(convBuffer, fftBuffer);
        VecOps::add(
            fftBuffer + inputBufferPos,
            overlapBuffer + inputBufferPos,
            output.section(samplesProcessed, samplesToProcess)
        );

        inputBufferPos += samplesToProcess;

        if (inputBufferPos == blockSize) {
            inputBuffer.zero();
            inputBufferPos = 0;

            fftBuffer.offset(blockSize).copyTo(overlapBuffer);
            currSegment = (currSegment > 0) ? currSegment - 1 : numBlocks - 1;
        }

        samplesProcessed += samplesToProcess;
    }
}

Buffer<float> ConvReverb::convolve(const Buffer<float>& inputFrq, Buffer<float> kernelFrq) {
    int paddedSize  = NumberUtils::nextPower2(kernelFrq.size() + inputFrq.size() - 1);
    int complexSize = paddedSize + 2;
    int cume        = 0;

    Transform& fft = getObj(Transforms).chooseFFT(paddedSize);
    Buffer<float> workBuffer = getObj(MemoryPool).getAudioPool();
    Buffer<float> kernelFrqPad = workBuffer.section(cume, complexSize);
    cume += complexSize;
    Buffer<float> inputFrqPad = workBuffer.section(cume, complexSize);
    cume += complexSize;

    kernelFrq.copyTo(kernelFrqPad + 2);
    kernelFrqPad.offset(2 + kernelFrq.size()).zero();

    inputFrq.copyTo(inputFrqPad + 2);
    inputFrqPad.offset(2 + inputFrq.size()).zero();

    // TODO does the cross-platform fft implementation pack the DC offset the same way?
    VecOps::mul(
        Buffer((Complex32*)kernelFrqPad.get() + 1, paddedSize / 2),
        Buffer((Complex32*)inputFrqPad.get() + 1, paddedSize / 2),
        Buffer((Complex32*)inputFrqPad.get() + 1, paddedSize / 2)
    );

    // ippsMul_32fc_I((Complex32*)kernelFrqPad.get() + 1,
    //                (Complex32*)inputFrqPad.get()  + 1, paddedSize / 2);

    fft.inverse(inputFrqPad);

    return inputFrqPad;
}

void ConvReverb::basicConvolve(
        Buffer<float> input,
        Buffer<float> response,
        Buffer<float> output) {
    jassert(output.size() >= input.size() + response.size() - 1);

    if(response.size() > input.size())
        std::swap(input, response);

    output.zero();
    for(int i = 0; i < response.size(); ++i) {
        for(int j = 0; j <= i; ++j) {
            output[i] += response[j] * input[i - j];
        }
    }

    for(int i = response.size(); i < input.size(); ++i) {
        for(int j = 0; j < response.size(); ++j) {
            output[i] += response[j] * input[i - j];
        }
    }

    for(int i = input.size(); i < input.size() + response.size() - 1; ++i){
        for(int j = i - input.size() + 1; j < response.size(); ++j) {
            output[i] += response[j] * input[i - j];
        }
    }
}

void ConvReverb::test() {
    test(44100, 131072, 512,    8192,   512,    false,  false);
    test(44100, 131072, 512,    8192,   512,    false,  true);
}

void ConvReverb::test(int inputSize, int irSize,
                      int headSize, int tailSize,
                      int bufferSize,
                      bool refCheck, bool useTwoStage) {
    int seed = (bufferSize ^ irSize ^ inputSize) % bufferSize;

    ScopedAlloc<float> memory(inputSize + irSize + bufferSize + (inputSize + irSize - 1) * 2);
    Buffer<float> in    = memory.place(inputSize);
    Buffer<float> ir    = memory.place(irSize);
    Buffer<float> buffer= memory.place(bufferSize);
    Buffer<float> out   = memory.place(inputSize + irSize - 1);
    Buffer<float> outCtrl = memory.place(inputSize + irSize - 1);

    memory.zero();

    Random random;

    in.zero();
    in.section(inputSize / 10, inputSize / 5).set(1.f); // .ramp(0.1f, 0.1f);
    in.section(inputSize / 4, inputSize / 2).set(-1.f); //ramp(-0.1f, -0.1f);
    in.front() = 1;
    ir.ramp(0.01f, 0.1f).inv().sin();

    if(refCheck) {
        basicConvolve(in, ir, outCtrl);
    }

    BlockConvolver simpleConvolver;

    useTwoStage ?
            init(headSize, tailSize, ir) :
            simpleConvolver.init(bufferSize, ir);

    int processedIn = 0, processedOut = 0;

    timer.start();

    while(processedOut < out.size()) {
        int maxToProcess    = jmax(1, seed % bufferSize); //jmax(1, random.nextInt(bufferSize - 1));
        int remainingOut    = out.size() - processedOut;
        int remainingIn     = in.size() - processedIn;
        int toProcessOut    = jmin(remainingOut, maxToProcess);
        int toProcessIn     = jmin(remainingIn, maxToProcess);

        seed = seed * 37 + 331;

        Buffer<float> subBuffer = buffer.withSize(toProcessIn);

        if(toProcessIn > 0)
            in.section(processedIn, toProcessIn).copyTo(subBuffer);

        useTwoStage ?
                process(subBuffer, out.section(processedOut, toProcessOut)) :
                simpleConvolver.process(subBuffer, out.section(processedOut, toProcessOut));

        processedOut += toProcessOut;
        processedIn += toProcessIn;
    }

    timer.stop();

    if (refCheck) {
        int     diffSamples     = 0;
        float   absTolerance    = 0.001f * float(ir.size());
        float   relTolerance    = 0.0001f * logf(float(ir.size()));

        for (int i = 0; i < outCtrl.size(); ++i) {
            float a = out[i];
            float b = outCtrl[i];

            if (fabsf(a) > 1 && fabsf(b) > 1) {
                float absError = fabsf(a - b);
                float relError = absError / b;

                if(relError > relTolerance && absError > absTolerance)
                    ++diffSamples;
            }
        }
    } else {
//      sout << "Performance Test:\t" << (useTwoStage ? "2-stage" : "1-stage") << " input " << inputSize << ", IR " << irSize << ", blocksize 1-"
//           << bufferSize << " => Clocks: " << (int) timer.getClocks() << ", clk/input: " << int(timer.getClocks() / inputSize)
//           << "\n";
    }
}

