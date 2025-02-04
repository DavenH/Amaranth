#pragma once

#include "ScopedAlloc.h"
#include "VecOps.h"

class ReadWriteBuffer {
public:
    ReadWriteBuffer() :
                readPosition        (0)
            ,   writePosition       (0)
            ,   totalSamplesWritten (0)
            ,   totalSamplesRead    (0) {
    }

    explicit ReadWriteBuffer(int size) :
            ReadWriteBuffer() {
        allocate(size);
    }

    explicit ReadWriteBuffer(Buffer<float> buff) :
            readPosition        (0)
        ,   writePosition       (0)
        ,   totalSamplesWritten (0)
        ,   totalSamplesRead    (0) {
        workBuffer = buff;
    }

    void allocate(int size) {
        memory.resize(size);
        workBuffer = memory;
    }

    void setMemoryBuffer(Buffer<float> memory) {
        workBuffer = memory;
    }


    Buffer<float> read(int size) {
        jassert(hasDataFor(size));

        Buffer<float> buf = (workBuffer + readPosition).withSize(size);
        readPosition += size;
        totalSamplesRead += size;

        return buf;
    }

    Buffer<float> readAtMost(int size) {
        size = jmin(size, writePosition - readPosition);
        return read(size);
    }

    Buffer<float> get(int size) {
        jassert(hasDataFor(size));

        return (workBuffer + readPosition).withSize(size);
    }

    float get() {
        jassert(hasDataFor(1));

        return workBuffer[readPosition];
    }

    bool hasRoomFor(int size) const {
        return writePosition + size <= workBuffer.size();
    }

    bool hasDataFor(int size) const {
        return writePosition - readPosition >= size;
    }

    void reset() {
        writePosition = 0;
        readPosition = 0;
        totalSamplesWritten = 0;
        totalSamplesRead = 0;
    }

    void write(float value) {
        if (writePosition + 1 > workBuffer.size()) {
            retract();
        }

        workBuffer[writePosition] = value;

        ++writePosition;
        ++totalSamplesWritten;
    }

    Buffer<float> write(Buffer<float> input) {
        if (input.empty())
            return {};

        jassert(input.size() <= workBuffer.size());

        if (writePosition + input.size() > workBuffer.size()) {
            retract();
        }

        Buffer output(workBuffer + writePosition, input.size());
        input.copyTo(output);

        totalSamplesWritten += input.size();
        writePosition       += input.size();

        return output;
    }

    float fromHistory(const int indicesBack) {
        if (writePosition - indicesBack < 0) {
            return 0.f;
        }

        return workBuffer[writePosition - indicesBack];
    }

    void retract(int amount) {
        if (writePosition < amount) {
            return;
        }

        VecOps::move(
            workBuffer.section(amount, writePosition),
            workBuffer.withSize(writePosition - amount)
        );

        writePosition -= amount;
    }

    void retract() {
        if (readPosition == 0) {
            return;
        }

        if (writePosition > readPosition) {
            VecOps::move(
                workBuffer.section(readPosition, writePosition - readPosition),
                workBuffer.withSize(writePosition - readPosition)
            );
        }
        writePosition -= readPosition;
        readPosition = 0;
    }

    ReadWriteBuffer& operator=(const ReadWriteBuffer& copy) {
        jassert(memory.empty());
        jassert(copy.memory.empty());

        totalSamplesRead    = copy.totalSamplesRead;
        totalSamplesWritten = copy.totalSamplesWritten;
        readPosition        = copy.readPosition;
        writePosition       = copy.writePosition;
        workBuffer          = copy.workBuffer;

        jassert(writePosition < workBuffer.size() || workBuffer.empty());

        return *this;
    }

    int readPosition;
    int writePosition;

    int64 totalSamplesWritten, totalSamplesRead;

    Buffer<float> workBuffer;
    ScopedAlloc<float> memory;
};
