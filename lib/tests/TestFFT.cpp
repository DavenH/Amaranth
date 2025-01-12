#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/Algo/FFT.h"
#include "../src/Array/Buffer.h"
#include "TestDefs.h"

TEST_CASE("Transform Initialization", "[transform]") {
    Transform fft;
    
    SECTION("Default construction") {
        // Should not crash or throw
        REQUIRE_NOTHROW(fft.allocate(1024));
    }
    
    SECTION("Multiple allocations") {
        REQUIRE_NOTHROW(fft.allocate(1024));
        REQUIRE_NOTHROW(fft.allocate(2048)); // Should handle reallocation
    }
    
    SECTION("Power of 2 sizes") {
        REQUIRE_NOTHROW(fft.allocate(256));
        REQUIRE_NOTHROW(fft.allocate(512));
        REQUIRE_NOTHROW(fft.allocate(1024));
    }
}

TEST_CASE("Transform Basic Operations", "[transform]") {
    Transform fft;
    const int size = 1024;
    fft.allocate(size, Transform::ScaleType::NoDivByAny);

    SECTION("DC Signal") {
        std::vector input(size, 1.0f); // DC signal
        Buffer buffer(input.data(), size);
        
        REQUIRE_NOTHROW(fft.forward(buffer));
        
        auto complex = fft.getComplex();
        REQUIRE(complex.size() > 0);
        // DC component should be size, all others near zero
        REQUIRE(real(complex[0]) == Catch::Approx(size).margin(0.1f));
        
        // Check other bins are near zero
        for (int i = 1; i < complex.size() / 2; ++i) {
            REQUIRE(mag(complex[i]) < 0.01f);
            REQUIRE(mag(complex[i]) < 0.01f);
        }
    }
    
    SECTION("Sine Wave") {
        std::vector<float> input(size);
        const float frequency = 4.0f; // 4 complete cycles
        for (int i = 0; i < size; ++i) {
            input[i] = std::sin(2.0f * M_PI * frequency * i / size);
        }
        
        Buffer<float> buffer(input.data(), size);
        fft.forward(buffer);
        
        auto complex = fft.getComplex();
        // Should have peaks at frequency bin 4 and size-4
        int freqBin = static_cast<int>(frequency);
        REQUIRE(mag(complex[freqBin]) > size/4);
    }
}

TEST_CASE("Transform Forward/Inverse", "[transform]") {
    Transform fft;
    const int size = 512;
    fft.allocate(size, Transform::DivFwdByN);
    
    SECTION("Identity Transform") {
        std::vector<float> input(size);
        // Generate random input signal
        for (int i = 0; i < size; ++i) {
            input[i] = static_cast<float>(rand()) / RAND_MAX;
        }
        std::vector<float> output(size);
        
        Buffer<float> inBuffer(input.data(), size);
        Buffer<float> outBuffer(output.data(), size);
        
        // Forward transform
        fft.forward(inBuffer);
        // Inverse transform
        fft.inverse(outBuffer);
        
        // Compare input and output
        for (int i = 0; i < size; ++i) {
            REQUIRE(input[i] == Catch::Approx(output[i]).margin(1e-5f));
        }
    }
}

TEST_CASE("Transform Scaling Options", "[transform]") {
    Transform fft;
    const int size = 256;

    SECTION("No Scaling") {
        fft.allocate(size, Transform::NoDivByAny);
        std::vector<float> input(size, 1.0f);
        Buffer<float> buffer(input.data(), size);
        
        fft.forward(buffer);
        auto complex = fft.getComplex();
        REQUIRE(std::abs(real(complex[0])) == Catch::Approx(size).margin(0.1f));
    }
    
    SECTION("Forward Scaling") {
        fft.allocate(size, Transform::DivFwdByN);
        std::vector<float> input(size, 1.0f);
        Buffer<float> buffer(input.data(), size);
        
        fft.forward(buffer);
        auto complex = fft.getComplex();
        REQUIRE(std::abs(real(complex[0])) == Catch::Approx(1.0f).margin(0.1f));
    }
}

TEST_CASE("Transform DC Offset Removal", "[transform]") {
    Transform fft;
    const int size = 256;
    fft.allocate(size, Transform::NoDivByAny);
    
    SECTION("With DC Removal") {
        fft.setRemovesOffset(true);
        std::vector input(size, 2.0f);
        Buffer buffer(input.data(), size);
        
        fft.forward(buffer);
        auto complex = fft.getComplex();
        REQUIRE(mag(complex[0]) < 0.1f); // DC should be removed
    }
    
    SECTION("Without DC Removal") {
        fft.setRemovesOffset(false);
        std::vector input(size, 2.0f);
        Buffer buffer(input.data(), size);
        
        fft.forward(buffer);
        auto complex = fft.getComplex();
        REQUIRE(std::abs(real(complex[0])) > 0.1f); // DC should be present
    }
}

TEST_CASE("Transform Cartesian Conversion", "[transform]") {
    Transform fft;
    const int size = 256;
    fft.allocate(size, Transform::DivFwdByN, true); // Enable cartesian conversion
    
    SECTION("Magnitude and Phase") {
        std::vector<float> input(size);
        // Generate a simple sine wave
        for (int i = 0; i < size; ++i) {
            input[i] = std::sin(2.0f * M_PI * i / size);
        }
        Buffer<float> buffer(input.data(), size);
        
        fft.forward(buffer);
        
        auto magnitudes = fft.getMagnitudes();
        auto phases = fft.getPhases();
        
        REQUIRE(magnitudes.size() > 0);
        REQUIRE(phases.size() > 0);
        
        // Check that magnitudes are non-negative
        for (int i = 0; i < magnitudes.size(); ++i) {
            REQUIRE(magnitudes[i] >= 0.0f);
        }
        
        // Check that phases are within [-π, π]
        for (int i = 0; i < phases.size(); ++i) {
            REQUIRE(phases[i] >= -M_PI - 0.01f);
            REQUIRE(phases[i] <= M_PI + 0.01f);
        }
    }
}

TEST_CASE("Transform Dirac Function", "[transform]") {
    Transform fft;
    const int size = 512;
    fft.allocate(size, Transform::NoDivByAny);

    SECTION("Dirac Impulse Response") {
        // Create dirac impulse (1 at t=0, 0 elsewhere)
        std::vector<float> input(size, 0.0f);
        input[0] = 1.0f;
        Buffer<float> buffer(input.data(), size);

        fft.forward(buffer);
        auto complex = fft.getComplex();

        // For a dirac function, all frequency bins should have equal magnitude
        float expectedMag = 1.0f;
        for (int i = 0; i < complex.size() / 2; ++i) {
            REQUIRE(mag(complex[i]) == Catch::Approx(expectedMag).margin(0.01f));
        }

        // Phases should be 0 for all bins since dirac is symmetric
        for (int i = 0; i < complex.size() / 2; ++i) {
            float phase = std::atan2(imag(complex[i]), real(complex[i]));
            REQUIRE(std::abs(phase) < 0.01f);
        }
    }
}

TEST_CASE("Transform Phase Shift Invariance", "[transform]") {
    Transform fft;
    const int size = 512;
    fft.allocate(size, Transform::DivFwdByN);

    SECTION("Time Domain Phase Shift") {
        // Create original signal (simple sine wave)
        std::vector<float> input1(size);
        std::vector<float> input2(size);
        std::vector<Complex32> freqData(size / 2);
        const float frequency = 4.0f;
        const float phaseShift = M_PI / 4.0f; // 45 degree shift

        Buffer<float> buffer1(input1.data(), size);
        Buffer<float> buffer2(input2.data(), size);
        Buffer<Complex32> storedCplx(freqData.data(), size);

        buffer1.ramp(0, 2 * M_PI * frequency / size).sin();
        buffer2.ramp(phaseShift, 2 * M_PI * frequency / size).sin();

        // Transform both signals
        fft.forward(buffer1);
        auto complex1 = fft.getComplex();
        complex1.copyTo(storedCplx);

        fft.forward(buffer2);
        auto complex2 = fft.getComplex();

        // Magnitudes should be identical despite phase shift
        for (int i = 0; i < complex1.size() / 2; ++i) {
            REQUIRE(mag(storedCplx[i]) == Catch::Approx(mag(complex2[i])).margin(0.01f));
        }

        // Phase difference at the fundamental frequency should match the applied shift
        int freqBin = static_cast<int>(frequency);
        float phase1 = std::atan2(imag(storedCplx[freqBin]), real(storedCplx[freqBin]));
        float phase2 = std::atan2(imag(complex2[freqBin]), real(complex2[freqBin]));

        // Normalize phase difference to [-π, π]
        float phaseDiff = phase2 - phase1;
        while (phaseDiff > M_PI) phaseDiff -= 2 * M_PI;
        while (phaseDiff < -M_PI) phaseDiff += 2 * M_PI;

        REQUIRE(std::abs(phaseDiff - phaseShift) < 0.01f);
    }
}
