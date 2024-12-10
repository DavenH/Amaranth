#pragma once
#include <vector>
#include "Buffer.h"
#include "ScopedAlloc.h"

using std::vector;

class IterableBuffer {
public:
    virtual ~IterableBuffer() = default;

    virtual bool hasNext() = 0;
    virtual Buffer<float> getNext() = 0;
    virtual void reset() = 0;

    virtual int getTotalSize() {
        int size = 0;

        reset();
        while (hasNext())
            size += getNext().size();

        reset();

        return size;
    }
};

class PitchedSample;

class SyncedSampleIterator : public IterableBuffer {
public:
    SyncedSampleIterator(PitchedSample* wrapper, Buffer<float> pitchBuffer);

    Buffer<float> getNext() override;
    void reset() override;
    bool hasNext() override;
    int getTotalSize() override;

private:
    bool hasReset;
    int sizePow2;
    long cycle;
    float realPosition;

    vector<double> periods;
    Buffer<float> pitchBuffer;

    PitchedSample* wav;
};

class WindowedSampleIterator : public IterableBuffer {
public:
    WindowedSampleIterator(Buffer<float> buffer, int bufferSize, int overlapFactor);

    Buffer<float> getNext() override;
    void reset() override;
    bool hasNext() override;
    int getTotalSize() override;

private:
    int overlapFactor, bufferSize;
    int numHarmonics, numColumns, index;

    long offset;

    ScopedAlloc<float> memory;
    Buffer<float> buffer, window, block;
};

class Column;

class BlockIterator : public IterableBuffer {
public:
    explicit BlockIterator(vector<Column>& columns);
    Buffer<float> getNext() override;
    bool hasNext() override;
    int getTotalSize() override;
    void reset() override;

private:
    int index;

    vector<Column>& columns;
};