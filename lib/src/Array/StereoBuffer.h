#pragma once

#include "Buffer.h"
#include "ScopedAlloc.h"

class StereoBuffer {
private:

public:
    int numChannels;
    ScopedAlloc<float> workBuffer;
    Buffer<float> left, right;

    explicit StereoBuffer(AudioBuffer<float>& audioBuffer) :
            numChannels(audioBuffer.getNumChannels()) {
        for (int i = 0; i < numChannels; ++i)
            (*this)[i] = Buffer<float>(audioBuffer, i);
    }

    StereoBuffer(AudioBuffer<float>& audioBuffer, int start, int size) :
            numChannels(audioBuffer.getNumChannels()) {
        left = Buffer(audioBuffer.getWritePointer(0, start), size);

        if (numChannels > 1)
            right = Buffer(audioBuffer.getWritePointer(1, start), size);
    }

    explicit StereoBuffer(const Buffer<float>& source) : numChannels(1) {
        left = Buffer(source);
    }

    explicit StereoBuffer(int channels) : numChannels(channels) {}

    void takeOwnership() {
        workBuffer.resize(numChannels * left.size());
        int size = left.size();
        for (int i = 0; i < numChannels; ++i) {
            Buffer<float> futureChannel = workBuffer.place(size);
            operator[](i).copyTo(futureChannel);
            operator[](i) = futureChannel;
        }
    }

    Buffer<float>& operator[](const int index) {
        switch (index) {
            case 0:
                return left;
            case 1:
                return right;
            default:
                jassertfalse;
        }
        return left;
    }

    [[nodiscard]] StereoBuffer section(int start, int size) const {
        StereoBuffer buffer(numChannels);
        buffer.left = left.sectionAtMost(start, size);

        if (numChannels > 1)
            buffer.right = right.sectionAtMost(start, size);

        return buffer;
    }

    StereoBuffer& add(const StereoBuffer& sb) {
        left.add(sb.left);

        if (numChannels > 1 && sb.numChannels > 1)
            right.add(sb.right);
        else if (numChannels > 1)
            right.add(sb.left);
        else if (sb.numChannels)
            left.add(sb.right);

        return *this;
    }

    StereoBuffer& mul(float factor) {
        left.mul(factor);

        if (numChannels > 1)
            right.mul(factor);

        return *this;
    }

    StereoBuffer& mul(const Buffer<float>& buff) {
        left.mul(buff);

        if (numChannels > 1)
            right.mul(buff);

        return *this;
    }

    StereoBuffer& mul(StereoBuffer& buff) {
        left.mul(buff.left);

        if (numChannels > 1 && buff.numChannels > 1)
            right.mul(buff.right);
        else if (numChannels > 1)
            right.mul(buff.left);


        return *this;
    }

    [[nodiscard]] float max() const {
        return jmax(left.max(), right.max());
    }

    void clear() {
        left.zero();

        if (numChannels > 1)
            right.zero();
    }

    [[nodiscard]] int size() const {
        return left.size();
    }

    [[nodiscard]] bool isEmpty() const {
        return left.empty();
    }
};
