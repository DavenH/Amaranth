#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/Algo/FFT.h"
#include "../src/Array/Buffer.h"
#include "TestDefs.h"

void print(const Buffer<Complex32>& buffer) {
    std::cout << std::fixed << std::setprecision(3);
    for (int i = 0; i < buffer.size(); ++i) {
        std::cout << i << "\t" << real(buffer[i]) << "\t" << imag(buffer[i]) << std::endl;
    }
}

void print(const Buffer<Float32>& buffer) {
    std::cout << std::fixed << std::setprecision(3);
    for (int i = 0; i < buffer.size(); ++i) {
        std::cout << i << "\t" << buffer[i] << std::endl;
    }
}

TEST_CASE("Transform Initialization", "[transform][init]") {
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

TEST_CASE("Transform Basic Operations", "[transform][sanity]") {
    Transform fft;
    const int size = 1024;
    fft.allocate(size, Transform::ScaleType::NoDivByAny);

    SECTION("DC Signal") {
        // DC signal
        ScopedAlloc<float> signal(size);
        signal.set(1);
        auto c1 = fft.getComplex();

        REQUIRE_NOTHROW(fft.forward(signal));
        
        auto complex = fft.getComplex();
        // print(complex);
        REQUIRE(complex.size() > 0);

        // DC component should be 'size', all others near zero
        REQUIRE(real(complex[0]) == Catch::Approx(size).margin(0.01f));
        
        // Check other bins are near zero
        for (int i = 1; i < complex.size() / 2; ++i) {
            REQUIRE(mag(complex[i]) < 0.01f);
            REQUIRE(mag(complex[i]) < 0.01f);
        }
    }
    
    SECTION("Sine Wave") {
        // 4 complete cycles
        const float frequency = 4.0f;
        ScopedAlloc<float> signal(size);
        signal.sin(frequency / size);
        fft.forward(signal);
        
        auto complex = fft.getComplex();
        // print(complex);

        // Should have peaks at frequency bin 4 and size-4
        int freqBin = roundToInt(frequency);
        REQUIRE(mag(complex[freqBin]) > size / 4);
    }
}

TEST_CASE("Transform Forward/Inverse", "[transform][identity]") {
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

TEST_CASE("Transform Scaling Options", "[transform][scaling]") {
    Transform fft;
    const int size = 256;

    SECTION("No Scaling") {
        fft.allocate(size, Transform::NoDivByAny);
        ScopedAlloc<float> signal(size);
        signal.set(1);

        fft.forward(signal);
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

TEST_CASE("Transform DC Offset Removal", "[transform][DC-offset]") {
    Transform fft;
    const int size = 256;
    fft.allocate(size, Transform::NoDivByAny);
    
    SECTION("With DC Removal") {
        fft.setRemovesOffset(true);
        ScopedAlloc<float> signal(size);
        signal.set(2.f);

        fft.forward(signal);
        auto complex = fft.getComplex();
        REQUIRE(mag(complex[0]) < 0.1f); // DC should be removed
    }
    
    SECTION("Without DC Removal") {
        fft.setRemovesOffset(false);
        ScopedAlloc<float> signal(size);
        signal.set(2.f);
        
        fft.forward(signal);
        auto complex = fft.getComplex();
        REQUIRE(std::abs(real(complex[0])) > 0.1f); // DC should be present
    }
}

TEST_CASE("Transform Cartesian Conversion", "[transform][polar-to-cart]") {
    Transform fft;
    const int size = 256;
    fft.allocate(size, Transform::DivFwdByN, true); // Enable cartesian conversion
    
    SECTION("Magnitude and Phase") {
        ScopedAlloc<float> signal(size);
        signal.sin(1.f);
        fft.forward(signal);
        
        auto magnitudes = fft.getMagnitudes();
        auto phases = fft.getPhases();
        
        REQUIRE(magnitudes.size() > 0);
        REQUIRE(phases.size() > 0);
        
        // Check that magnitudes are non-negative
        for (float magnitude : magnitudes) {
            REQUIRE(magnitude >= 0.0f);
        }
        
        // Check that phases are within [-π, π]
        for (float phase : phases) {
            REQUIRE(phase >= -M_PI - 0.01f);
            REQUIRE(phase <= M_PI + 0.01f);
        }
    }
}

TEST_CASE("Transform Standard Functions", "[transform][functions]") {
    Transform fft;
    const int size = 512;
    fft.allocate(size, Transform::NoDivByAny, true);

    SECTION("Dirac Impulse Response") {
        // Create dirac impulse (1 at t=0, 0 elsewhere)
        ScopedAlloc<float> signal(size);
        signal.zero();
        signal[0] = 1.f;

        fft.forward(signal);
        auto complex = fft.getComplex();
        // For a dirac function, all frequency bins should have equal magnitude
        float expectedMag = 1.0f;
        // skip dc/nyquist
        for (int i = 1; i < complex.size() / 2 + 1; ++i) {
            REQUIRE(mag(complex[i]) == Catch::Approx(expectedMag).margin(0.01f));
        }

        // Phases should be 0 for all bins since dirac is symmetric
        for (int i = 1; i < complex.size() / 2 + 1;  ++i) {
            float phase = std::atan2(imag(complex[i]), real(complex[i]));
            REQUIRE(std::abs(phase) < 0.01f);
        }
    }

    SECTION("Sawtooth Harmonic Series") {
        // Create sawtooth wave [-1, 1]
        ScopedAlloc<float> signal(size);
        signal.ramp(-1, 2.f / static_cast<float>(size - 1));
        // print(signal);

        fft.forward(signal);
        auto complex = fft.getComplex();
        print(complex);

        auto magnitudes = fft.getMagnitudes();

        // In a sawtooth wave, the nth harmonic should have magnitude proportional to 1/n
        // We'll check the first 8 harmonics
        const int numHarmonicsToCheck = 8;

        // The ratio between successive harmonics should be approximately n/(n+1)
        // We allow for some deviation due to windowing effects and finite resolution
        for (int n = 0; n < numHarmonicsToCheck - 1; ++n) {
            float expectedRatio = static_cast<float>(n + 1) / static_cast<float>(n + 2);
            float actualRatio = magnitudes[n + 1] / magnitudes[n];

            // Use a relatively generous margin due to discretization and windowing effects
            REQUIRE(actualRatio == Catch::Approx(expectedRatio).margin(0.15f));
        }

        // Additionally, check that harmonics decay approximately as 1/n
        float firstHarmonicMag = magnitudes[0];
        for (int n = 1; n < numHarmonicsToCheck; ++n) {
            float expectedMag = firstHarmonicMag / (n + 1);
            REQUIRE(magnitudes[n] == Catch::Approx(expectedMag).margin(0.15f));
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

TEST_CASE("Transform Sawtooth Harmonics", "[transform]") {
    Transform fft;
    const int size = 1024; // Larger size for better frequency resolution
    fft.allocate(size, Transform::NoDivByAny, true); // Enable cartesian conversion for magnitude computation

}