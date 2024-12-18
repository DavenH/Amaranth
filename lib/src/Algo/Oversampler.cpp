#include <ipp.h>
#include "Oversampler.h"
#include "../App/SingletonRepo.h"
#include "../App/MemoryPool.h"
#include "../Definitions.h"


Oversampler::Oversampler(SingletonRepo* repo, int kernelSize) :
        memoryBuf		(getObj(MemoryPool).getAudioPool())
    ,	oversampleFactor(1)
    ,	filterDownState	(nullptr)
    ,	filterUpState	(nullptr)
    ,	oversampBuf		(nullptr)
    ,	phase			(0) {
    setKernelSize(2 * kernelSize);
}

Oversampler::~Oversampler() {
    filterUpState = nullptr;
    filterDownState = nullptr;
}

void Oversampler::start(Buffer<float>& buffer) {
    if (oversampleFactor > 1) {
        int destSize = oversampleFactor * buffer.size();

        phase = 0;
        audioBuf = buffer;

        jassert(memoryBuf.size() >= oversampleFactor * buffer.size());

        buffer.mul(oversampleFactor * 0.97f);
        ippsSampleUp_32f(buffer, buffer.size(), memoryBuf, &destSize, oversampleFactor, &phase);

        int partitionPos = 0;

        while (partitionPos < destSize) {
            int stepSize = jmin(destSize - partitionPos, 1024);

            Buffer partition(memoryBuf + partitionPos, stepSize);

            ippsFIRSR_32f(partition, partition, stepSize, filterUpState, firUpDly, firUpDly, workBuffUp);
            ippsThreshold_LTAbs_32f_I(partition, stepSize, 1e-12f);

            partitionPos += stepSize;
        }

        buffer 		= memoryBuf.withSize(destSize); // Buffer<Ipp32f>(memoryBuf, destSize);
        oversampBuf = &buffer;
    } else {
        oversampBuf = nullptr;
    }
}

void Oversampler::stop() {
    if (oversampleFactor > 1 && oversampBuf != 0 && oversampBuf->size() > 0 && audioBuf.size() > 0) {
        jassert(filterDownState);
        jassert(oversampBuf->size() > 0 && audioBuf.size() > 0);

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

    int partitionPos 	= 0;
    int destPos 		= 0;

    while(partitionPos < src.size()) {
        int srcStep 	= jmin((int) src.size() - partitionPos, 1024);
        int destStep 	= srcStep / oversampleFactor;
        int destSize 	= destStep;

        Buffer partition(src + partitionPos, srcStep);
        Buffer destPart(dest + destPos, destStep);

        ippsFIRSR_32f		(partition, partition, srcStep, filterDownState, firDownDly, firDownDly, workBuffDown);
        ippsSampleDown_32f	(partition, srcStep, destPart, &destSize, oversampleFactor, &phase);

        partitionPos 	+= srcStep;
        destPos 		+= destStep;

        jassert(destSize == destStep);
    }

    if (wrapTail) {
        int destSize;
        Buffer<float> temp = memoryBuf.withSize(firDownDly.size());
        Buffer<float> tail = memoryBuf.section(firDownDly.size(), firDownDly.size());

        tail.zero();

        ippsFIRSR_32f		(tail, tail, tail.size(), filterDownState, firDownDly, firDownDly, workBuffDown);
        ippsSampleDown_32f	(tail, tail.size(), temp, &destSize, oversampleFactor, &phase);

        dest.add(temp.withSize(destSize));
    }
}

void Oversampler::setOversampleFactor(int factor) {
    oversampleFactor = factor;

    updateTaps();
}

void Oversampler::setKernelSize(int size) {
    firTaps		.resize(size);
    firUpDly	.resize(size);
    firDownDly	.resize(size);

    firDownDly.zero();
    firUpDly.zero();

    int buffSize, specSize;
    ippsFIRSRGetSize(firTaps.size(), ipp32f, &specSize, &buffSize);

    stateUpBuf.resize(specSize);
    stateDownBuf.resize(specSize);

    ippsFIRSRInit_32f(firTaps, firTaps.size(), ippAlgAuto, filterUpState);
    ippsFIRSRInit_32f(firTaps, firTaps.size(), ippAlgAuto, filterDownState);

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
    if(oversampleFactor == 1)
        return;

    double relativeFreq = 0.5 / (double) oversampleFactor;

    ScopedAlloc<Ipp64f> tempTaps(firTaps.size());

    int buffSize;
    ippsFIRGenGetBufferSize(firTaps.size(), &buffSize);
    ScopedAlloc<Ipp8u> buffer(buffSize);

    ippsFIRGenLowpass_64f(relativeFreq, tempTaps, (int)firTaps.size(), ippWinBlackman, ippTrue, buffer);
    ippsConvert_64f32f	 (tempTaps, firTaps, firTaps.size());

    ippsFIRSRInit_32f(firTaps, firTaps.size(), IppAlgType::ippAlgAuto, filterUpState);
    ippsFIRSRInit_32f(firTaps, firTaps.size(), IppAlgType::ippAlgAuto, filterDownState);
}

Buffer<float> Oversampler::getMemoryBuffer(int size) {
    jassert(size <= memoryBuf.size());

    return memoryBuf.withSize(size);
}
