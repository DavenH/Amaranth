#pragma once

#include "Buffer.h"

// Non-owning channel view. Copies alias the same underlying sample memory.
class StereoBuffer {
public:
    StereoBuffer() = default;

    StereoBuffer(const StereoBuffer&) = default;
    StereoBuffer& operator=(const StereoBuffer&) = default;
    StereoBuffer(StereoBuffer&&) noexcept = default;
    StereoBuffer& operator=(StereoBuffer&&) noexcept = default;

    explicit StereoBuffer(AudioBuffer<float>& audioBuffer) :
            numChannels(jmin(2, audioBuffer.getNumChannels())) {
        for (int i = 0; i < numChannels; ++i) {
            (*this)[i] = Buffer<float>(audioBuffer, i);
        }
    }

    StereoBuffer(AudioBuffer<float>& audioBuffer, int start, int size) :
            numChannels(jmin(2, audioBuffer.getNumChannels())) {
        left = Buffer(audioBuffer.getWritePointer(0, start), size);

        if (numChannels > 1) {
            right = Buffer(audioBuffer.getWritePointer(1, start), size);
        }
    }

    explicit StereoBuffer(const Buffer<float>& source) : numChannels(1) {
        left = Buffer(source);
    }

    StereoBuffer(const Buffer<float>& left, const Buffer<float>& right) :
            numChannels(right.empty() ? 1 : 2)
        ,   left(left)
        ,   right(right) {
        jassert(right.empty() || left.size() == right.size());
    }

    explicit StereoBuffer(int channels) : numChannels(channels) {}

    Buffer<float>& operator[](const int index) {
        switch (index) {
            case 0: return left;
            case 1: return right;
            default:
                jassertfalse;
                break;
        }
        return left;
    }

    const Buffer<float>& operator[](const int index) const {
        switch (index) {
            case 0: return left;
            case 1: return right;
            default:
                jassertfalse;
                break;
        }
        return left;
    }

    [[nodiscard]] StereoBuffer section(int start, int size) const {
        StereoBuffer buffer(numChannels);
        buffer.left = left.sectionAtMost(start, size);

        if (numChannels > 1) {
            buffer.right = right.sectionAtMost(start, size);
        }

        return buffer;
    }

    StereoBuffer& add(const StereoBuffer& sb) {
        left.add(sb.left);

        if (numChannels > 1 && sb.numChannels > 1) {
            right.add(sb.right);
        } else if (numChannels > 1) {
            right.add(sb.left);
        } else if (sb.numChannels > 1) {
            left.add(sb.right);
        }

        return *this;
    }

    StereoBuffer& mul(float factor) {
        left.mul(factor);

        if (numChannels > 1) {
            right.mul(factor);
        }

        return *this;
    }

    StereoBuffer& mul(const Buffer<float>& buff) {
        left.mul(buff);

        if (numChannels > 1) {
            right.mul(buff);
        }

        return *this;
    }

    StereoBuffer& mul(const StereoBuffer& buff) {
        left.mul(buff.left);

        if (numChannels > 1 && buff.numChannels > 1) {
            right.mul(buff.right);
        } else if (numChannels > 1) {
            right.mul(buff.left);
        }

        return *this;
    }

    [[nodiscard]] float max() const {
        if (isEmpty()) {
            return 0.f;
        }

        return numChannels > 1 ? jmax(left.max(), right.max()) : left.max();
    }

    void clear() {
        left.zero();

        if (numChannels > 1) {
            right.zero();
        }
    }

    [[nodiscard]] int size() const {
        return left.size();
    }

    [[nodiscard]] bool isEmpty() const {
        return left.empty();
    }

    int numChannels{};
    Buffer<float> left;
    Buffer<float> right;
};
