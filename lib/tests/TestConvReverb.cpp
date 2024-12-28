#include <catch2/catch_test_macros.hpp>
#include "JuceHeader.h"
using namespace juce;
#include "../src/Algo/ConvReverb.h"
#include "../src/App/SingletonRepo.h"
#include "../src/Array/ScopedAlloc.h"

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
    SingletonRepo repo;

    ConvReverb reverb(&repo);

    SECTION("Initialization with valid parameters") {
        const int headSize = 512;
        const int tailSize = 8192;
        const int kernelSize = 16384;
        
        ScopedAlloc<float> kernelAlloc(kernelSize);
        Buffer<float> kernel(kernelAlloc.get(), kernelSize);
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
    SingletonRepo repo;
    ConvReverb reverb(&repo);

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

    SECTION("Two-stage convolution accuracy") {
        const int inputSize = 44100;
        const int irSize = 131072;
        const int headSize = 512;
        const int tailSize = 8192;
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
    SingletonRepo repo;
    ConvReverb reverb(&repo);

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