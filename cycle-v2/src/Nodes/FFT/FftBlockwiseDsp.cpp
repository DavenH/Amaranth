#include "FftBlockwiseDsp.h"

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

    if (isPowerOfTwo(preparedFrameCount)) {
        transform.allocate((int) preparedFrameCount, Transform::ScaleType::DivFwdByN, true);
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
}

Buffer<float> FftBlockwiseDsp::blockBuffer(const AudioProcessBlock& block, size_t size) const {
    return { const_cast<float*>(block.samples.data()), (int) size };
}

Buffer<float> FftBlockwiseDsp::writableBlockBuffer(AudioProcessBlock& block, size_t size) const {
    return { block.samples.data(), (int) size };
}

int FftBlockwiseDsp::binCount() const {
    return (int) preparedFrameCount / 2;
}

}
