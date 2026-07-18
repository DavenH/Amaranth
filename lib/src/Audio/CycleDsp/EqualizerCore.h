#pragma once

#include <Array/Buffer.h>

#include <memory>
#include <vector>

namespace CycleDsp {

class EqualizerCore {
public:
    static constexpr int bandCount = 5;

    explicit EqualizerCore(int channelCount);
    ~EqualizerCore();

    void configureBand(int bandIndex, double sampleRate, float frequency, float gainDecibels);
    void process(int channel, Buffer<float> samples);
    void clear();
    float responseDecibels(double frequency) const;

private:
    class Band;

    int channelCount;
    double sampleRate { 44100.0 };
    std::vector<std::unique_ptr<Band>> bands;
};

}
