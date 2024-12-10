#ifndef CIRCLEBUFFER_H_
#define CIRCLEBUFFER_H_

#include "Buffer.h"

class CircleBuffer : public Buffer<float> {
public:
    CircleBuffer() {
        init();
    }

    CircleBuffer(const Buffer<float>& buff) : Buffer<float>(buff) {
        jassert(!(sz & (sz - 1)));
        init();
    }

    void init() {
        writePosition = 0;
        readPosition = 0;

        zero();
    }

    virtual ~CircleBuffer() {}

    void write(float value) {
        ptr[writePosition++ & (sz - 1)] = value;
    }

    float read(int delay) {
        return ptr[(readPosition - delay) & (sz - 1)];
    }


    float read() {
        jassert(readPosition < writePosition);
        return ptr[readPosition++ & (sz - 1)];
    }

    int readArray(Buffer<float> dest) {
//		jassert(readPosition + dest.size() <= writePosition);

        int moddedRead = readPosition & (sz - 1);
        int maxReadable = int(writePosition - readPosition);
        int maxTotal = jmin(dest.size(), maxReadable);
        int serialReadSize = sz - moddedRead;

        section(moddedRead, jmin(serialReadSize, maxTotal)).copyTo(dest);

        if (serialReadSize < maxTotal)
            section(0, maxTotal - serialReadSize).copyTo(dest + serialReadSize);

        readPosition += maxTotal;
        return maxTotal;
    }

private:
    unsigned int writePosition, readPosition;

};

#endif
