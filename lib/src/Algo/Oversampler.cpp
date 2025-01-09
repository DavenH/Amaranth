#include "Oversampler.h"
#include "../App/SingletonRepo.h"
#include "../App/MemoryPool.h"
#include "../Definitions.h"

Oversampler::Oversampler(SingletonRepo* repo, int kernelSize) :
        memoryBuf        (getObj(MemoryPool).getAudioPool())
    ,   oversampleFactor (1)
  #ifdef USE_IPP
    ,   filterDownState  (nullptr)
    ,   filterUpState    (nullptr)
  #endif
    ,   oversampBuf      (nullptr)
    ,   phase            (0) {
    setKernelSize(2 * kernelSize);
}

Oversampler::~Oversampler() {
  #ifdef USE_IPP
    filterUpState = nullptr;
    filterDownState = nullptr;
  #endif
}

void Oversampler::start(Buffer<float>& buffer) {
    if (oversampleFactor > 1) {
        int destSize = oversampleFactor * buffer.size();

        phase = 0;
        audioBuf = buffer;

        jassert(memoryBuf.size() >= oversampleFactor * buffer.size());

        buffer.mul(oversampleFactor * 0.97f);
        phase = memoryBuf
            .withSize(oversampleFactor * buffer.size())
            .upsampleFrom(buffer, oversampleFactor, phase);

        int partitionPos = 0;

        while (partitionPos < destSize) {
            int stepSize = jmin(destSize - partitionPos, 1024);

            Buffer partition(memoryBuf + partitionPos, stepSize);

          #ifdef USE_IPP
            ippsFIRSR_32f(partition, partition, stepSize, filterUpState, firUpDly, firUpDly, workBuffUp);
          #else
            vDSP_desamp(partition, 1, firTaps, partition, partition.size(), firTaps.size());
          #endif
            partition.add(1e-11f);

            partitionPos += stepSize;
        }

        buffer = memoryBuf.withSize(destSize); // Buffer<Float32>(memoryBuf, destSize);
        oversampBuf = &buffer;
    } else {
        oversampBuf = nullptr;
    }
}

void Oversampler::stop() {
    if (oversampleFactor > 1 && oversampBuf != nullptr && !oversampBuf->empty() && !audioBuf.empty()) {
        jassert(!oversampBuf->empty() && !audioBuf.empty());

        sampleDown(*oversampBuf, audioBuf);

        // so clients can continue processing with the buffer they passed to start()
        *oversampBuf = audioBuf;
    }
}

void Oversampler::sampleDown(Buffer<float> src, Buffer<float> dest, bool wrapTail) {
    if (oversampleFactor == 1) {
        src.copyTo(dest);
        return;
    }

    int partitionPos = 0;
    int destPos      = 0;

    while(partitionPos < src.size()) {
        int srcStep  = jmin(src.size() - partitionPos, 1024);
        int destStep = srcStep / oversampleFactor;
        int destSize = destStep;

        Buffer partition(src + partitionPos, srcStep);
        Buffer destPart(dest + destPos, destStep);

      #ifdef USE_IPP
        jassert(filterDownState);
        ippsFIRSR_32f(partition, partition, srcStep, filterDownState, firDownDly, firDownDly, workBuffDown);
      #else
        vDSP_desamp(partition, 1, firTaps, partition, partition.size(), firTaps.size());
      #endif
        phase = destPart.downsampleFrom(partition, oversampleFactor, phase);

        partitionPos += srcStep;
        destPos      += destStep;

        jassert(destSize == destStep);
    }

    if (wrapTail) {
        int destSize;
        Buffer<float> temp = memoryBuf.withSize(firDownDly.size());
        Buffer<float> tail = memoryBuf.section(firDownDly.size(), firDownDly.size());

        tail.zero();

      #ifdef USE_IPP
        ippsFIRSR_32f(tail, tail, tail.size(), filterDownState, firDownDly, firDownDly, workBuffDown);
      #else
        vDSP_desamp(tail, 1, firTaps, tail, tail.size(), firTaps.size());
      #endif
        phase = temp.downsampleFrom(tail, oversampleFactor, phase);

        dest.add(temp.withSize(destSize));
    }
}

void Oversampler::setOversampleFactor(int factor) {
    oversampleFactor = factor;

    updateTaps();
}

void Oversampler::setKernelSize(int size) {
    firTaps     .resize(size);
    firUpDly    .resize(size);
    firDownDly  .resize(size);

    firDownDly.zero();
    firUpDly.zero();

    int buffSize, specSize;

#ifdef USE_IPP
    ippsFIRSRGetSize(firTaps.size(), ipp32f, &specSize, &buffSize);

    stateUpBuf.resize(specSize);
    stateDownBuf.resize(specSize);

    ippsFIRSRInit_32f(firTaps, firTaps.size(), ippAlgAuto, filterUpState);
    ippsFIRSRInit_32f(firTaps, firTaps.size(), ippAlgAuto, filterDownState);
#endif

    updateTaps();
}

Buffer<float> Oversampler::getTail() {
    return firDownDly;
}

int Oversampler::getLatencySamples() {
    return oversampleFactor > 1 ? firTaps.size() / oversampleFactor : 0;
}

void Oversampler::resetDelayLine() {
    firDownDly.zero();
    firUpDly.zero();
}

void Oversampler::updateTaps() {
    if(oversampleFactor == 1) {
        return;
    }

    double relativeFreq = 0.5 / (double) oversampleFactor;

  #ifdef USE_IPP
    int buffSize;
    ippsFIRGenGetBufferSize(firTaps.size(), &buffSize);
    ScopedAlloc<Int8u> buffer(buffSize);

    ScopedAlloc<Float64> tempTaps(firTaps.size());
    ippsFIRGenLowpass_64f(relativeFreq, tempTaps, firTaps.size(), ippWinBlackman, ippTrue, buffer);
    VecOps::convert(tempTaps, firTaps);

    ippsFIRSRInit_32f(firTaps, firTaps.size(), ippAlgAuto, filterUpState);
    ippsFIRSRInit_32f(firTaps, firTaps.size(), ippAlgAuto, filterDownState);
  #else
    firTaps.sinc(relativeFreq);
  #endif
}

Buffer<float> Oversampler::getMemoryBuffer(int size) {
    jassert(size <= memoryBuf.size());

    return memoryBuf.withSize(size);
}
