#include <ipps.h>
#include "Convolver.h"
#include "../App/SingletonRepo.h"
#include "../App/Transforms.h"
#include "../Util/Arithmetic.h"
#include "../Util/NumberUtils.h"
#include "../App/MemoryPool.h"
#include "../Definitions.h"

Convolver::Convolver(SingletonRepo* repo) : SingletonAccessor(repo, "Convolver") {
}

void Convolver::setKernel(Buffer<float> kernel) {
    this->kernel = kernel;
}

Buffer<float> Convolver::processBuffer(Buffer<float> buffer) {
    return Buffer<float>();
}

Buffer<float> Convolver::convolve(
        Buffer<float> kernelFrq,
        Buffer<float> inputFrq) {
    int paddedSize = NumberUtils::nextPower2(kernelFrq.size() + inputFrq.size() - 1) + 2;

    Transform& spec = getObj(Transforms).chooseFFT(paddedSize);

    int cume = 0;
    Buffer<float> workBuffer = getObj(MemoryPool).getAudioPool();
    Buffer<float> kernelFrqPad = workBuffer.section(cume, paddedSize);
    cume += paddedSize;
    Buffer<float> inputFrqPad = workBuffer.section(cume, paddedSize);
    cume += paddedSize;

    kernelFrq.copyTo(kernelFrqPad + 2);
    kernelFrqPad.offset(2 + kernelFrq.size()).zero();

    inputFrq.copyTo(inputFrqPad + 2);
    inputFrqPad.offset(2 + inputFrq.size()).zero();

    ippsMul_32fc_I((Ipp32fc*) kernelFrqPad.get() + 1,
                   (Ipp32fc*) inputFrqPad.get() + 1, (paddedSize - 2) / 2);

    spec >> inputFrqPad;

    return inputFrqPad;
}
