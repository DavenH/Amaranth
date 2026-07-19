#include <App/MemoryPool.h>
#include <catch2/catch_test_macros.hpp>
#include "JuceHeader.h"
using namespace juce;
#include "../src/Algo/ConvReverb.h"
#include "../src/Array/ScopedAlloc.h"
#include "../src/Audio/CycleDsp/ReverbKernel.h"

class TestConvReverb {
public:
    // Helper method to verify convolution results
    static bool verifyConvolution(const Buffer<float>& output, 
                                const Buffer<float>& reference,
                                float absTolerance,
                                float relTolerance) {
        int diffSamples = 0;
        
        for (int i = 0; i < reference.size(); ++i) {
            float actual = output[i];
            float expected = reference[i];

            if (fabsf(actual) > 1 && fabsf(expected) > 1) {
                float absError = fabsf(actual - expected);
                float relError = absError / expected;

                if (relError > relTolerance && absError > absTolerance) {
                    ++diffSamples;
                }
            }
        }
        
        return diffSamples == 0;
    }

    // Helper to generate test signal
    static void generateTestSignal(Buffer<float>& buffer) {
        buffer.zero();
        buffer.section(buffer.size() / 10, buffer.size() / 5).set(1.0f);
        buffer.section(buffer.size() / 4, buffer.size() / 2).set(-1.0f);
        buffer.front() = 1.0f;
    }

    // Helper to generate test impulse response
    static void generateTestIR(Buffer<float>& buffer) {
        buffer.ramp(0.01f, 0.1f).inv().sin();
    }
};

TEST_CASE("ConvReverb Basic Operation", "[ConvReverb]") {
    ConvReverb reverb;

    SECTION("Initialization with valid parameters") {
        const int headSize = 64;
        const int tailSize = 256;
        const int kernelSize = 1024;
        
        ScopedAlloc<float> kernelAlloc(kernelSize);
        Buffer kernel(kernelAlloc.get(), kernelSize);
        TestConvReverb::generateTestIR(kernel);

        REQUIRE_NOTHROW(reverb.init(headSize, tailSize, kernel));
    }

    SECTION("Reset functionality") {
        REQUIRE_NOTHROW(reverb.reset());
        // Verify internal state is reset
        CHECK(reverb.headBlockSize == 0);
        CHECK(reverb.tailBlockSize == 0);
        CHECK(reverb.precalcPos == 0);
        CHECK(reverb.tailInputPos == 0);
    }
}

TEST_CASE("ConvReverb Convolution Accuracy", "[ConvReverb]") {
    ConvReverb reverb;

    SECTION("Single-stage convolution accuracy") {
        const int inputSize = 44100;
        const int irSize = 4096;
        const int bufferSize = 512;
        
        ScopedAlloc<float> memory(inputSize + irSize + bufferSize + (inputSize + irSize - 1) * 2);
        Buffer<float> input = memory.place(inputSize);
        Buffer<float> ir = memory.place(irSize);
        Buffer<float> buffer = memory.place(bufferSize);
        Buffer<float> output = memory.place(inputSize + irSize - 1);
        Buffer<float> reference = memory.place(inputSize + irSize - 1);

        // Generate test signals
        TestConvReverb::generateTestSignal(input);
        TestConvReverb::generateTestIR(ir);

        // Generate reference result
        ConvReverb::basicConvolve(input, ir, reference);

        // Test single-stage convolution
        BlockConvolver convolver;
        convolver.init(bufferSize, ir);

        int processedIn = 0, processedOut = 0;
        while (processedOut < output.size()) {
            int toProcess = std::min(bufferSize, output.size() - processedOut);
            Buffer<float> subBuffer = buffer.withSize(toProcess);

            if (processedIn < input.size()) {
                int inputRemaining = std::min(toProcess, input.size() - processedIn);
                input.section(processedIn, inputRemaining).copyTo(subBuffer);
                processedIn += inputRemaining;
            }

            convolver.process(subBuffer, output.section(processedOut, toProcess));
            processedOut += toProcess;
        }

        float absTolerance = 0.001f * float(ir.size());
        float relTolerance = 0.0001f * logf(float(ir.size()));
        
        CHECK(TestConvReverb::verifyConvolution(output, reference, absTolerance, relTolerance));
    }

    SECTION("Dirac impulse response preserves amplitude") {
        const int inputSize = 512;
        const int bufferSize = 64;
        const int irSize = bufferSize;

        ScopedAlloc<float> memory(inputSize + irSize + bufferSize + inputSize);
        Buffer<float> input = memory.place(inputSize);
        Buffer<float> ir = memory.place(irSize);
        Buffer<float> buffer = memory.place(bufferSize);
        Buffer<float> output = memory.place(inputSize);

        input.ramp(-1.f, 2.f / float(inputSize - 1));
        ir.zero();
        ir.front() = 1.f;
        output.zero();

        BlockConvolver convolver;
        convolver.init(bufferSize, ir);

        int processed = 0;
        while (processed < inputSize) {
            int toProcess = std::min(bufferSize, inputSize - processed);
            Buffer<float> subBuffer = buffer.withSize(toProcess);

            input.section(processed, toProcess).copyTo(subBuffer);
            convolver.process(subBuffer, output.section(processed, toProcess));

            processed += toProcess;
        }

        for (int i = 0; i < inputSize; ++i) {
            CHECK(fabsf(output[i] - input[i]) <= 1e-4f);
        }
    }

    SECTION("Two-stage convolution accuracy") {
        const int inputSize = 44100;
        const int irSize = 4096;
        const int headSize = 16;
        const int tailSize = 1024;
        const int bufferSize = 256;
        
        ScopedAlloc<float> memory(inputSize + irSize + bufferSize + (inputSize + irSize - 1) * 2);
        Buffer<float> input = memory.place(inputSize);
        Buffer<float> ir = memory.place(irSize);
        Buffer<float> buffer = memory.place(bufferSize);
        Buffer<float> output = memory.place(inputSize + irSize - 1);
        Buffer<float> reference = memory.place(inputSize + irSize - 1);

        // Generate test signals
        TestConvReverb::generateTestSignal(input);
        TestConvReverb::generateTestIR(ir);

        // Generate reference result
        ConvReverb::basicConvolve(input, ir, reference);

        // Initialize two-stage convolution
        reverb.init(headSize, tailSize, ir);

        int processedIn = 0, processedOut = 0;
        while (processedOut < output.size()) {
            int toProcess = std::min(bufferSize, output.size() - processedOut);
            Buffer<float> subBuffer = buffer.withSize(toProcess);

            if (processedIn < input.size()) {
                int inputRemaining = std::min(toProcess, input.size() - processedIn);
                input.section(processedIn, inputRemaining).copyTo(subBuffer);
                processedIn += inputRemaining;
            }

            reverb.process(subBuffer, output.section(processedOut, toProcess));
            processedOut += toProcess;
        }

        float absTolerance = 0.001f * float(ir.size());
        float relTolerance = 0.0001f * logf(float(ir.size()));
        
        CHECK(TestConvReverb::verifyConvolution(output, reference, absTolerance, relTolerance));
    }
}

TEST_CASE("ConvReverb Edge Cases", "[ConvReverb]") {
    ConvReverb reverb;

    SECTION("Empty input handling") {
        const int bufferSize = 512;
        ScopedAlloc<float> buffer(bufferSize);
        Buffer<float> emptyInput(nullptr, 0);
        Buffer<float> output(buffer.get(), bufferSize);

        REQUIRE_NOTHROW(reverb.process(emptyInput, output));
    }

    SECTION("Zero kernel size") {
        const int headSize = 512;
        const int tailSize = 8192;
        Buffer<float> emptyKernel(nullptr, 0);

        REQUIRE_NOTHROW(reverb.init(headSize, tailSize, emptyKernel));
    }

    SECTION("Invalid block sizes") {
        const int kernelSize = 1024;
        ScopedAlloc<float> kernelAlloc(kernelSize);
        Buffer<float> kernel(kernelAlloc.get(), kernelSize);
        TestConvReverb::generateTestIR(kernel);

        // Test with zero head size
        REQUIRE_NOTHROW(reverb.init(0, 8192, kernel));
        
        // Test with zero tail size
        REQUIRE_NOTHROW(reverb.init(512, 0, kernel));
        
        // Test with head size larger than tail size
        REQUIRE_NOTHROW(reverb.init(8192, 512, kernel));
    }
}

TEST_CASE("Cycle reverb kernel generation is deterministic and stereo", "[ConvReverb]") {
    constexpr int kernelSize = 4096;
    ScopedAlloc<float> memory(kernelSize * 4);
    Buffer<float> firstLeft = memory.place(kernelSize);
    Buffer<float> firstRight = memory.place(kernelSize);
    Buffer<float> secondLeft = memory.place(kernelSize);
    Buffer<float> secondRight = memory.place(kernelSize);

    CycleDsp::ReverbKernelConfiguration configuration;
    configuration.roomSize = 0.35f;
    configuration.damping = 0.14f;
    configuration.highPass = 0.05f;
    CycleDsp::buildReverbKernel(configuration, firstLeft, firstRight);
    CycleDsp::buildReverbKernel(configuration, secondLeft, secondRight);

    for (int i = 0; i < kernelSize; ++i) {
        REQUIRE(firstLeft[i] == secondLeft[i]);
        REQUIRE(firstRight[i] == secondRight[i]);
    }
    REQUIRE_FALSE(firstLeft == firstRight);
    REQUIRE_FALSE(firstLeft.isProbablyEmpty());
    REQUIRE_FALSE(firstRight.isProbablyEmpty());
}

TEST_CASE("Cycle reverb high pass attenuates low bins independently of damping",
        "[ConvReverb][ReverbKernel]") {
    constexpr int kernelSize = 4096;
    constexpr int lowBinCount = 96;
    constexpr int highBinStart = 512;
    constexpr int highBinCount = 512;
    ScopedAlloc<float> memory(kernelSize * 4);
    Buffer<float> unfiltered = memory.place(kernelSize);
    Buffer<float> highPassed = memory.place(kernelSize);
    Buffer<float> scratchRight = memory.place(kernelSize);
    Buffer<float> unfilteredMagnitudes = memory.place(kernelSize);

    CycleDsp::ReverbKernelConfiguration configuration;
    configuration.roomSize = 0.35f;
    configuration.damping = 0.f;
    configuration.highPass = 0.f;
    CycleDsp::buildReverbKernel(configuration, unfiltered, scratchRight);
    configuration.highPass = 1.f;
    CycleDsp::buildReverbKernel(configuration, highPassed, scratchRight);

    Transform transform;
    transform.allocate(kernelSize, Transform::DivFwdByN, true);
    transform.forward(unfiltered);
    transform.getMagnitudes().copyTo(unfilteredMagnitudes);
    transform.forward(highPassed);
    Buffer<float> highPassedMagnitudes = transform.getMagnitudes();

    const float lowRetention = highPassedMagnitudes.withSize(lowBinCount).sum()
            / unfilteredMagnitudes.withSize(lowBinCount).sum();
    const float lowestRetention = highPassedMagnitudes.withSize(24).sum()
            / unfilteredMagnitudes.withSize(24).sum();
    const float highRetention = highPassedMagnitudes
            .section(highBinStart, highBinCount)
            .sum()
            / unfilteredMagnitudes.section(highBinStart, highBinCount).sum();
    INFO("low-bin retention: " << lowRetention);
    INFO("lowest-bin retention: " << lowestRetention);
    INFO("high-bin retention: " << highRetention);
    REQUIRE(lowestRetention < 0.5f);
    REQUIRE(lowRetention < 0.9f);
    REQUIRE(highRetention > 0.9f);
}
