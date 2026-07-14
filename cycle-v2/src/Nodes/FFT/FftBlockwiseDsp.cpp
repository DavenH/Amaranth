#include "FftBlockwiseDsp.h"

#include <Array/VecOps.h>

namespace CycleV2 {

namespace {

bool isPowerOfTwo(size_t value) {
    return value > 0 && (value & (value - 1)) == 0;
}

void copySpectralRows(
        const AudioProcessBlock& source,
        size_t sourceOffset,
        size_t destOffset,
        Buffer<float> dest) {
    if (source.samples.size() <= sourceOffset || (size_t) dest.size() <= destOffset) {
        return;
    }

    const size_t copyCount = std::min((size_t) dest.size() - destOffset, source.samples.size() - sourceOffset);
    Buffer<float>(
            const_cast<float*>(source.samples.data()) + sourceOffset,
            (int) copyCount).copyTo({
                    dest.get() + destOffset,
                    (int) copyCount
            });
}

}

void FftBlockwiseDsp::prepare(size_t frameCount) {
    if (frameCount == preparedFrameCount) {
        return;
    }

    preparedFrameCount = frameCount;
    resetState();

    if (isPowerOfTwo(preparedFrameCount)) {
        transform.allocate((int) preparedFrameCount, Transform::ScaleType::DivFwdByN, true);
        allocateHalfCycleCarry();
        const size_t fullBinCount = (size_t) transform.getFullRealBinCount();
        scratchMagnitude.resize(fullBinCount);
        scratchPhase.resize(fullBinCount);
    }
}

void FftBlockwiseDsp::resetState() {
    hasCarry = false;

    if (carryHalf) {
        carryHalf.zero();
    }

    if (rawHalf) {
        rawHalf.zero();
    }
}

void FftBlockwiseDsp::setHalfCycleCarryEnabled(bool shouldEnable) {
    if (halfCycleCarryEnabled == shouldEnable) {
        return;
    }

    halfCycleCarryEnabled = shouldEnable;
    resetState();
}

void FftBlockwiseDsp::forward(
        const AudioProcessBlock& input,
        AudioProcessBlock& magnitude,
        AudioProcessBlock& phase) {
    prepare(input.samples.size());

    const size_t fullBinCount = (size_t) transform.getFullRealBinCount();
    magnitude.samples.resize(fullBinCount);
    phase.samples.resize(fullBinCount);
    writableBlockBuffer(magnitude, magnitude.samples.size()).zero();
    writableBlockBuffer(phase, phase.samples.size()).zero();

    if (!isPowerOfTwo(preparedFrameCount)) {
        return;
    }

    transform.forward(blockBuffer(input, preparedFrameCount));
    transform.copyFullPolarSpectrumTo(
            writableBlockBuffer(magnitude, magnitude.samples.size()),
            writableBlockBuffer(phase, phase.samples.size()));
}

void FftBlockwiseDsp::inverse(
        const AudioProcessBlock& magnitude,
        const AudioProcessBlock* phase,
        AudioProcessBlock& output) {
    prepare(output.samples.size());

    output.samples.resize(preparedFrameCount);

    if (!isPowerOfTwo(preparedFrameCount)) {
        writableBlockBuffer(output, preparedFrameCount).zero();
        return;
    }

    const size_t fullBinCount = (size_t) transform.getFullRealBinCount();
    scratchMagnitude.resize(fullBinCount);
    scratchPhase.resize(fullBinCount);
    Buffer<float> magnitudeBuffer(scratchMagnitude.data(), (int) scratchMagnitude.size());
    Buffer<float> phaseBuffer(scratchPhase.data(), (int) scratchPhase.size());
    magnitudeBuffer.zero();
    phaseBuffer.zero();

    const bool hasDcRow = magnitude.samples.size() >= fullBinCount;
    copySpectralRows(magnitude, 0, hasDcRow ? 0 : 1, magnitudeBuffer);

    if (phase != nullptr) {
        copySpectralRows(*phase, 0, hasDcRow ? 0 : 1, phaseBuffer);
    }

    transform.setFullPolarSpectrum(magnitudeBuffer, phaseBuffer);
    transform.inverse(writableBlockBuffer(output, preparedFrameCount));
    applyHalfCycleCarry(output);
}

Buffer<float> FftBlockwiseDsp::blockBuffer(const AudioProcessBlock& block, size_t size) const {
    return { const_cast<float*>(block.samples.data()), (int) size };
}

Buffer<float> FftBlockwiseDsp::writableBlockBuffer(AudioProcessBlock& block, size_t size) const {
    return { block.samples.data(), (int) size };
}

void FftBlockwiseDsp::applyHalfCycleCarry(AudioProcessBlock& output) {
    if (!halfCycleCarryEnabled) {
        return;
    }

    const int half = binCount();

    if (half <= 0 || carryHalf.empty()) {
        return;
    }

    Buffer<float> outputHalf = writableBlockBuffer(output, (size_t) half);
    outputHalf.copyTo(rawHalf);

    if (hasCarry) {
        outputHalf.mul(fadeIn);
        outputHalf.addProduct(fadeOut, carryHalf);
    }

    rawHalf.copyTo(carryHalf);
    hasCarry = true;
}

void FftBlockwiseDsp::allocateHalfCycleCarry() {
    const int half = binCount();

    if (half <= 0) {
        carryMemory.clear();
        carryHalf.nullify();
        rawHalf.nullify();
        fadeIn.nullify();
        fadeOut.nullify();
        return;
    }

    carryMemory.ensureSize(4 * half);
    carryHalf = carryMemory.place(half);
    rawHalf = carryMemory.place(half);
    fadeIn = carryMemory.place(half);
    fadeOut = carryMemory.place(half);

    carryHalf.zero();
    rawHalf.zero();

    if (half == 1) {
        fadeIn.set(1.f);
        fadeOut.zero();
        return;
    }

    fadeIn.ramp(
            -0.5f * MathConstants<float>::pi,
            MathConstants<float>::pi / (float) (half - 1))
            .sin()
            .add(1.f)
            .mul(0.5f);
    VecOps::subCRev(fadeIn, 1.f, fadeOut);
}

int FftBlockwiseDsp::binCount() const {
    return (int) preparedFrameCount / 2;
}

}
