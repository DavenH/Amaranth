#include <catch2/catch_test_macros.hpp>

#include <Algo/FFT.h>
#include <App/SingletonRepo.h>

#include "../Equalizer.h"

namespace
{
float getMagnitude(const Complex32& value)
{
  #ifdef USE_ACCELERATE
    return std::abs(value);
  #else
    return std::sqrt(value.re * value.re + value.im * value.im);
  #endif
}

void fillSawtooth(Buffer<float> dest, float sampleRate, float frequency)
{
    const float phaseInc = frequency / sampleRate;
    float phase = 0.0f;

    for (int i = 0; i < dest.size(); ++i) {
        dest[i] = 2.0f * phase - 1.0f;
        phase += phaseInc;

        if (phase >= 1.0f) {
            phase -= 1.0f;
        }
    }
}

float getBinMagnitude(Buffer<float> signal, float sampleRate, float frequencyHz)
{
    Transform fft;
    fft.allocate(signal.size(), Transform::DivFwdByN);
    fft.forward(signal);

    Buffer<Complex32> spectrum = fft.getComplex();
    const int maxBin = spectrum.size() / 2 - 1;
    const int targetBin = jlimit(1, maxBin, roundToInt(frequencyHz * signal.size() / sampleRate));

    return getMagnitude(spectrum[targetBin]);
}
}

TEST_CASE("Equalizer Shapes Sawtooth Spectrum", "[cycle][equalizer][fft]")
{
    constexpr int fftSize = 4096;
    constexpr float sampleRate = 44100.0f;
    constexpr float sawFrequency = 100.0f;
    constexpr float targetFrequency = 1200.0f;
    constexpr float referenceFrequency = 5000.0f;

    SingletonRepo repo;
    Equalizer equalizer(&repo);
    equalizer.setSampleRate(sampleRate);

    AudioSampleBuffer baselineBuffer(1, fftSize);
    AudioSampleBuffer processedBuffer(1, fftSize);

    Buffer<float> baseline(baselineBuffer.getWritePointer(0), fftSize);
    Buffer<float> processed(processedBuffer.getWritePointer(0), fftSize);

    fillSawtooth(baseline, sampleRate, sawFrequency);
    baseline.copyTo(processed);

    SECTION("Boosting the mid band increases target harmonics more than far harmonics")
    {
        REQUIRE(equalizer.paramChanged(Equalizer::Band3Gain, 0.9, true));
        equalizer.processBuffer(processedBuffer);

        const float beforeTarget = getBinMagnitude(baseline, sampleRate, targetFrequency);
        const float afterTarget = getBinMagnitude(processed, sampleRate, targetFrequency);
        const float beforeReference = getBinMagnitude(baseline, sampleRate, referenceFrequency);
        const float afterReference = getBinMagnitude(processed, sampleRate, referenceFrequency);

        const float targetRatio = afterTarget / jmax(beforeTarget, 1.0e-6f);
        const float referenceRatio = afterReference / jmax(beforeReference, 1.0e-6f);

        REQUIRE(targetRatio > 1.1f);
        REQUIRE(targetRatio > referenceRatio * 1.2f);
    }

    SECTION("Cutting the mid band reduces target harmonics")
    {
        REQUIRE(equalizer.paramChanged(Equalizer::Band3Gain, 0.1, true));
        equalizer.processBuffer(processedBuffer);

        const float beforeTarget = getBinMagnitude(baseline, sampleRate, targetFrequency);
        const float afterTarget = getBinMagnitude(processed, sampleRate, targetFrequency);

        const float targetRatio = afterTarget / jmax(beforeTarget, 1.0e-6f);
        REQUIRE(targetRatio < 0.9f);
    }
}
