#include "Column.h"
#include "StereoBuffer.h"
#include "IterableBuffer.h"
#include "../Algo/Resampling.h"
#include "../Audio/PitchedSample.h"

SyncedSampleIterator::SyncedSampleIterator(PitchedSample* wrapper, Buffer<float> pitchBuffer) : wav(wrapper)
    ,   pitchBuffer(pitchBuffer)
    ,   hasReset(false)
    ,   cycle(0)
    ,   realPosition(0) {
    float average = pitchBuffer.mean();
    sizePow2 = nextPowerOfTwo(roundToInt(average));
}

bool SyncedSampleIterator::hasNext() {
    return cycle < periods.size();
}

void SyncedSampleIterator::reset() {
    cycle           = 0;
    realPosition    = 0;
    double offset   = 0;

    int numSamples   = wav->audio.size();

    periods.clear();

    while (true) {
        double progress = offset / double(numSamples);
        double period   = wav->samplerate / Resampling::lerpC(pitchBuffer, progress);

        periods.push_back(period);
        offset += period;

        if ((int) offset >= wav->audio.size()) {
            break;
        }
    }

    hasReset = true;
}

Buffer<float> SyncedSampleIterator::getNext() {
    float position = realPosition / wav->samplerate;

    int floorOffset = roundToInt(realPosition);
    int size        = roundToInt(realPosition + periods[cycle]) - floorOffset;

    realPosition += periods[cycle++];

    return wav->audio.left.sectionAtMost(floorOffset, size);
}

int SyncedSampleIterator::getTotalSize() {
    jassert(hasReset);

    return periods.size() * sizePow2;
}

BlockIterator::BlockIterator(vector<Column>& columns)
        : columns(columns), index(0) {
}

bool BlockIterator::hasNext() {
    return index < columns.size();
}

void BlockIterator::reset() {
    index = 0;
}

Buffer<float> BlockIterator::getNext() {
    jassert(isPositiveAndBelow(index, (int) columns.size()));

    return columns[index];
}

int BlockIterator::getTotalSize() {
    if (columns.empty())
        return 0;

    return columns.size() * columns.front().size();
}

WindowedSampleIterator::WindowedSampleIterator(Buffer<float> buffer, int bufferSize, int overlapFactor) :
        buffer(buffer)
    ,   bufferSize(bufferSize)
    ,   overlapFactor(overlapFactor) {
    numHarmonics = bufferSize / 2;
    numColumns  = overlapFactor * buffer.size() / bufferSize;

    memory.resize(bufferSize * 2);
    window = memory.place(bufferSize);
    block = memory.place(bufferSize);

    window.hann();

    jassert(overlapFactor > 0);
}

bool WindowedSampleIterator::hasNext() {
    return offset < buffer.size();
}

Buffer<float> WindowedSampleIterator::getNext() {
    block.zero();
    buffer.sectionAtMost(offset, bufferSize).copyTo(block);
    block.mul(window);

    offset += bufferSize / overlapFactor;

    return block;
}

void WindowedSampleIterator::reset() {
    offset = 0;
}

int WindowedSampleIterator::getTotalSize() {
    if (buffer.empty()) {
        return 0;
    }

    return overlapFactor * buffer.size();
}
