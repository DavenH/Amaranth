#include "FftBlockwiseDsp.h"

#include <Array/VecOps.h>

namespace CycleV2 {

namespace {

bool isPowerOfTwo(size_t value) {
    return value > 0 && (value & (value - 1)) == 0;
}

}

void FftBlockwiseDsp::prepare(size_t frameCount) {
    if (frameCount == preparedFrameCount) {
        return;
    }

    preparedFrameCount = frameCount;
    hasCarry = false;

    if (isPowerOfTwo(preparedFrameCount)) {
        transform.allocate((int) preparedFrameCount, Transform::ScaleType::DivFwdByN, true);
        allocateHalfCycleCarry();
    }
}

void FftBlockwiseDsp::forward(
        const AudioProcessBlock& input,
        AudioProcessBlock& magnitude,
        AudioProcessBlock& phase) {
    prepare(input.samples.size());

    magnitude.samples.resize(preparedFrameCount);
    phase.samples.resize(preparedFrameCount);
    writableBlockBuffer(magnitude, preparedFrameCount).zero();
    writableBlockBuffer(phase, preparedFrameCount).zero();

    if (!isPowerOfTwo(preparedFrameCount)) {
        return;
    }

    transform.forward(blockBuffer(input, preparedFrameCount));

    const int bins = binCount();
    transform.getMagnitudes().withSize(bins).copyTo(writableBlockBuffer(magnitude, bins));
    transform.getPhases().withSize(bins).copyTo(writableBlockBuffer(phase, bins));
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

    const int bins = binCount();
    transform.getMagnitudes().zero();
    transform.getPhases().zero();
    blockBuffer(magnitude, bins).copyTo(transform.getMagnitudes().withSize(bins));

    if (phase != nullptr) {
        blockBuffer(*phase, bins).copyTo(transform.getPhases().withSize(bins));
    }

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
