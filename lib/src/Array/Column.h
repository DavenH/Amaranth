#pragma once

#include "Buffer.h"

class Column : public Buffer<float> {
public:
    char midiKey{};
    int latency{};
    float averagePower{};
    float x{};

    Column(float* array, int size, float x, char midiKey) :
            Buffer(array, size), averagePower(1), midiKey(midiKey), latency(0), x(x) {
    }

    explicit Column(const Buffer& buff) : Buffer(buff) {
        init();
    }

    Column() {
        init();
    }

    void init() {
        averagePower = 1;
        midiKey = -1;
        x = 0;
        latency = 0;
    }

    Buffer withSize(int size) {
        jassert(size <= sz);

        return {ptr, size};
    }

    ~Column() override = default;

    Column& operator=(const Buffer& buff) {
        this->ptr = buff.get();
        this->sz = buff.size();
        return *this;
    }

private:
    JUCE_LEAK_DETECTOR(Column)
};
