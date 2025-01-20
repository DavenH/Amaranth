#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Array/Buffer.h"
#include "TestDefs.h"
#include <array>
#include <cmath>

using Catch::Approx;

// Helper function to compare floating point arrays
template<typename T>
bool compareBuffers(const Buffer<T>& a, const Buffer<T>& b, T tolerance = 1e-6) {
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); i++) {
        if (std::abs(a[i] - b[i]) > tolerance) return false;
    }
    return true;
}

TEST_CASE("Buffer basic operations", "[buffer]") {
    SECTION("Constructor and basic accessors") {
        std::array<float, 4> data = {1.0f, 2.0f, 3.0f, 4.0f};
        Buffer<float> buf(data.data(), 4);
        
        REQUIRE(buf.size() == 4);
        REQUIRE_FALSE(buf.empty());
        REQUIRE(buf[0] == 1.0f);
        REQUIRE(buf[3] == 4.0f);
        REQUIRE(buf.front() == 1.0f);
        REQUIRE(buf.back() == 4.0f);
    }

    SECTION("Buffer views and sections") {
        std::array<float, 6> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        Buffer<float> buf(data.data(), 6);
        
        auto section = buf.section(2, 3);
        REQUIRE(section.size() == 3);
        REQUIRE(section[0] == 3.0f);
        REQUIRE(section[2] == 5.0f);
        
        auto offset = buf.offset(2);
        REQUIRE(offset.size() == 4);
        REQUIRE(offset[0] == 3.0f);
        
        auto sized = buf.withSize(4);
        REQUIRE(sized.size() == 4);
        REQUIRE(sized[3] == 4.0f);
    }
}

TEST_CASE("Buffer initialization operations", "[buffer]") {
    std::array<float, 4> data = {1.0f, 2.0f, 3.0f, 4.0f};
    
    SECTION("zero") {
        Buffer<float> buf(data.data(), 4);
        buf.zero();
        for (float i : buf) {
            REQUIRE(i == 0.0f);
        }
    }
    
    SECTION("ramp") {
        Buffer<float> buf(data.data(), 4);
        buf.ramp();
        REQUIRE(buf[0] == 0.0f);
        REQUIRE(buf[buf.size()-1] == 1.0f);
        REQUIRE(buf[2] == Approx(2.0f/3.0f));
    }
    
    SECTION("custom ramp") {
        Buffer<float> buf(data.data(), 4);
        buf.ramp(1.0f, 2.0f);
        REQUIRE(buf[0] == 1.0f);
        REQUIRE(buf[1] == 3.0f);
        REQUIRE(buf[2] == 5.0f);
        REQUIRE(buf[3] == 7.0f);
    }
}

TEST_CASE("Buffer mathematical operations", "[buffer]") {
    std::array<float, 4> data1{};
    std::array<float, 4> data2{};

    Buffer<float> buf1, buf2;

    auto beforeEach = [&]() {
        data1 = {1.0f, 2.0f, 3.0f, 4.0f};
        data2 = {2.0f, 3.0f, 4.0f, 5.0f};

        buf1 = Buffer(data1.data(), 4);
        buf2 = Buffer(data2.data(), 4);
    };


    SECTION("add") {
        beforeEach();

        buf1.add(buf2);
        REQUIRE(buf1[0] == 3.0f);
        REQUIRE(buf1[3] == 9.0f);
    }
    
    SECTION("mul") {
        beforeEach();

        buf1.mul(buf2);
        REQUIRE(buf1[0] == 2.0f);
        REQUIRE(buf1[3] == 20.0f);
    }
    
    SECTION("scalar operations") {
        beforeEach();

        buf1.mul(2.0f);
        REQUIRE(buf1[0] == 2.0f);
        REQUIRE(buf1[3] == 8.0f);
        
        buf1.add(1.0f);
        REQUIRE(buf1[0] == 3.0f);
        REQUIRE(buf1[3] == 9.0f);
    }
}

TEST_CASE("Buffer statistical operations", "[buffer][statistics]") {
    std::array<float, 4> data = {1.0f, 2.0f, 3.0f, 4.0f};
    Buffer buf(data.data(), 4);
    
    SECTION("basic statistics") {
        REQUIRE(buf.min() == 1.0f);
        REQUIRE(buf.max() == 4.0f);
        REQUIRE(buf.mean() == 2.5f);
        REQUIRE(buf.sum() == 10.0f);
    }
    
    SECTION("L1 and L2 norms") {
        REQUIRE(buf.normL1() == 10.0f);
        REQUIRE(buf.normL2() == Approx(std::sqrt(30.0f)));
    }
    
    SECTION("standard deviation") {
        REQUIRE(buf.stddev() == Approx(std::sqrt(5/3.)));
    }
}

TEST_CASE("Buffer threshold operations", "[buffer]") {
    std::array<float, 4> data{};
    Buffer<float> buf;

    auto beforeEach = [&]() {
        data = {-1.0f, 0.5f, 2.0f, 3.5f};
        buf = Buffer(data.data(), 4);
    };

    SECTION("clip") {
        beforeEach();
        buf.clip(0.0f, 2.0f);
        REQUIRE(buf[0] == 0.0f);
        REQUIRE(buf[1] == 0.5f);
        REQUIRE(buf[2] == 2.0f);
        REQUIRE(buf[3] == 2.0f);
    }
    
    SECTION("threshold operations") {
        beforeEach();
        buf.threshLT(1.0f);
        REQUIRE(buf[0] == 1.0f);
        REQUIRE(buf[1] == 1.0f);
        REQUIRE(buf[2] == 2.0f);
        REQUIRE(buf[3] == 3.5f);
        
        beforeEach();
        buf.threshGT(1.0f);
        REQUIRE(buf[0] == -1.0f);
        REQUIRE(buf[1] == 0.5f);
        REQUIRE(buf[2] == 1.0f);
        REQUIRE(buf[3] == 1.0f);
    }
}

TEST_CASE("Buffer edge cases", "[buffer]") {
    SECTION("empty buffer") {
        Buffer<float> buf;
        REQUIRE(buf.empty());
        REQUIRE(buf.size() == 0);
        REQUIRE(buf.isProbablyEmpty());
    }
    
    SECTION("single element") {
        float value = 1.0f;
        Buffer<float> buf(&value, 1);
        REQUIRE(buf.size() == 1);
        REQUIRE(buf.front() == buf.back());
        REQUIRE(buf[0] == 1.0f);
    }
    
    SECTION("division by zero") {
        std::array<float, 4> data = {1.0f, 2.0f, 3.0f, 4.0f};
        Buffer<float> buf(data.data(), 4);
        buf.div(0.0f); // a no-op
        REQUIRE(buf[0] == 1.0f);
        REQUIRE(buf[3] == 4.0f);
    }
}

TEST_CASE("Buffer arithmetic operators", "[buffer][arithmetic]") {
    std::array<float, 4> data1{};
    std::array<float, 4> data2{};

    Buffer<float> buf1, buf2;

    auto beforeEach = [&]() {
        data1 = {1.0f, 2.0f, 3.0f, 4.0f};
        data2 = {0.5f, 1.5f, 2.5f, 3.5f};

        buf1 = Buffer(data1.data(), 4);
        buf2 = Buffer(data2.data(), 4);
    };

    SECTION("operator+=") {
        beforeEach();

        buf1 += buf2;
        REQUIRE(buf1[0] == 1.5f);
        REQUIRE(buf1[1] == 3.5f);
        REQUIRE(buf1[2] == 5.5f);
        REQUIRE(buf1[3] == 7.5f);

        // Test scalar addition
        buf1 += 1.0f;
        REQUIRE(buf1[0] == 2.5f);
        REQUIRE(buf1[1] == 4.5f);
        REQUIRE(buf1[2] == 6.5f);
        REQUIRE(buf1[3] == 8.5f);
    }

    SECTION("operator-=") {
        beforeEach();

        buf1 -= buf2;
        REQUIRE(buf1[0] == 0.5f);
        REQUIRE(buf1[1] == 0.5f);
        REQUIRE(buf1[2] == 0.5f);
        REQUIRE(buf1[3] == 0.5f);

        // Test scalar subtraction
        buf1 -= 0.5f;
        REQUIRE(buf1[0] == 0.0f);
        REQUIRE(buf1[1] == 0.0f);
        REQUIRE(buf1[2] == 0.0f);
        REQUIRE(buf1[3] == 0.0f);
    }

    SECTION("operator*=") {
        beforeEach();

        buf1 *= buf2;
        REQUIRE(buf1[0] == 0.5f);
        REQUIRE(buf1[1] == 3.0f);
        REQUIRE(buf1[2] == 7.5f);
        REQUIRE(buf1[3] == 14.0f);

        // Test scalar multiplication
        buf1 *= 2.0f;
        REQUIRE(buf1[0] == 1.0f);
        REQUIRE(buf1[1] == 6.0f);
        REQUIRE(buf1[2] == 15.0f);
        REQUIRE(buf1[3] == 28.0f);
    }

    SECTION("operator/=") {
        beforeEach();

        buf1 /= buf2;
        REQUIRE(buf1[0] == 2.0f);
        REQUIRE(buf1[1] == Approx(1.333333f));
        REQUIRE(buf1[2] == 1.2f);
        REQUIRE(buf1[3] == Approx(1.142857f));

        // Test scalar division
        buf1 /= 2.0f;
        REQUIRE(buf1[0] == 1.0f);
        REQUIRE(buf1[1] == Approx(0.666667f));
        REQUIRE(buf1[2] == 0.6f);
        REQUIRE(buf1[3] == Approx(0.571429f));
    }
}


TEST_CASE("Buffer advanced arithmetic operations", "[buffer][arithmetic]") {
    Buffer<float> buf1, buf2;
    std::array<float, 4> data1{};
    std::array<float, 4> data2{};

    auto beforeEach = [&]() {
        data1 = {1.0f, 2.0f, 3.0f, 4.0f};
        data2 = {0.5f, 1.5f, 2.5f, 3.5f};

        buf1 = Buffer(data1.data(), 4);
        buf2 = Buffer(data2.data(), 4);
    };

    SECTION("addProduct with scalar") {
        beforeEach();

        buf1.addProduct(buf2, 2.0f);  // buf1[i] += buf2[i] * 2.0f
        REQUIRE(buf1[0] == 2.0f);     // 1.0 + (0.5 * 2.0)
        REQUIRE(buf1[1] == 5.0f);     // 2.0 + (1.5 * 2.0)
        REQUIRE(buf1[2] == 8.0f);     // 3.0 + (2.5 * 2.0)
        REQUIRE(buf1[3] == 11.0f);    // 4.0 + (3.5 * 2.0)
    }

    SECTION("addProduct with two buffers") {
        beforeEach();

        std::array<float, 4> data3 = {1.0f, 1.0f, 1.0f, 1.0f};
        Buffer result(data3.data(), 4);

        result.addProduct(buf1, buf2);  // result[i] += buf1[i] * buf2[i]
        REQUIRE(result[0] == 1.5f);     // 1.0 + (1.0 * 0.5)
        REQUIRE(result[1] == 4.0f);     // 1.0 + (2.0 * 1.5)
        REQUIRE(result[2] == 8.5f);     // 1.0 + (3.0 * 2.5)
        REQUIRE(result[3] == 15.0f);    // 1.0 + (4.0 * 3.5)
    }

    SECTION("reverse operations") {
        beforeEach();
        buf1.subCRev(5.0f);  // buf[i] = 5.0f - buf[i]
        REQUIRE(buf1[0] == 4.0f);
        REQUIRE(buf1[1] == 3.0f);
        REQUIRE(buf1[2] == 2.0f);
        REQUIRE(buf1[3] == 1.0f);

        beforeEach();
        buf1.divCRev(12.0f);  // buf[i] = 12.0f / buf[i]
        REQUIRE(buf1[0] == 12.0f);
        REQUIRE(buf1[1] == 6.0f);
        REQUIRE(buf1[2] == 4.0f);
        REQUIRE(buf1[3] == 3.0f);

        beforeEach();
        buf1 = Buffer(data1.data(), 4);
        buf1.powCRev(2.0f);  // buf[i] = 2.0f ** buf[i]
        REQUIRE(buf1[0] == 2.0f);
        REQUIRE(buf1[1] == 4.0f);
        REQUIRE(buf1[2] == 8.0f);
        REQUIRE(buf1[3] == 16.0f);
    }
}

TEST_CASE("Buffer arithmetic edge cases", "[buffer][arithmetic]") {

    SECTION("division by zero") {
        const std::array<float, 4> original = {1.0f, 2.0f, 3.0f, 4.0f};
        std::array<float, 4> copy1 = original;
        std::array<float, 4> copy2 = original;
        Buffer buf(copy1.data(), 4);
        Buffer zero(copy2.data(), 4);
        zero.zero();

        // Test scalar division by zero
        buf /= 0.0f;  // Should be a no-op
        REQUIRE(buf[0] == 1.0f);
        REQUIRE(buf[1] == 2.0f);
        REQUIRE(buf[2] == 3.0f);
        REQUIRE(buf[3] == 4.0f);

        // Test division by zero buffer
        buf /= zero;
        REQUIRE(platformSplit(isinf(buf[0]), isinf(buf[0]), isinff(buf[0])));
    }

    SECTION("operations with different sized buffers") {
        std::array<float, 4> data1 = {1.0f, 2.0f, 3.0f, 4.0f};
        std::array<float, 2> data2 = {0.5f, 1.5f};

        Buffer<float> buf1(data1.data(), 4);
        Buffer<float> buf2(data2.data(), 2);

        // Operations should assert in debug builds
        #ifdef NDEBUG
        buf1 += buf2;  // In release builds, should only affect valid elements
        REQUIRE(buf1[0] == 1.5f);
        REQUIRE(buf1[1] == 3.5f);
        REQUIRE(buf1[2] == 3.0f);
        REQUIRE(buf1[3] == 4.0f);
        #endif
    }
}

#ifdef USE_ACCELERATE
  #define VIMAGE_H
  #include <Accelerate/Accelerate.h>
#endif

TEST_CASE("Buffer utility operations", "[buffer][utility]") {
    Buffer<float> buf1, buf2;
    std::array<float, 4> data1{};
    std::array<float, 4> data2{};

    auto beforeEach = [&]() {
        data1 = {1.0f, 2.0f, 3.0f, 4.0f};
        data2 = {0.5f, 1.5f, 2.5f, 3.5f};
        buf1 = Buffer(data1.data(), 4);
        buf2 = Buffer(data2.data(), 4);
    };

    SECTION("flip") {
        beforeEach();

        buf1.flip();
        REQUIRE(buf1[0] == 4.0f);
        REQUIRE(buf1[1] == 3.0f);
        REQUIRE(buf1[2] == 2.0f);
        REQUIRE(buf1[3] == 1.0f);

        // Test flip on odd length buffer
        std::array<float, 3> data3 = {1.0f, 2.0f, 3.0f};
        Buffer buf3(data3.data(), 3);
        buf3.flip();
        REQUIRE(buf3[0] == 3.0f);
        REQUIRE(buf3[1] == 2.0f);
        REQUIRE(buf3[2] == 1.0f);
    }

    SECTION("withPhase") {
        beforeEach();

        std::array<float, 4> workData{};
        Buffer work(workData.data(), 4);

        // Test positive phase shift
        buf1.withPhase(1, work);
        REQUIRE(buf1[0] == 2.0f);
        REQUIRE(buf1[1] == 3.0f);
        REQUIRE(buf1[2] == 4.0f);
        REQUIRE(buf1[3] == 1.0f);
        buf1.withPhase(1, work);
        REQUIRE(buf1[0] == 3.0f);
        REQUIRE(buf1[1] == 4.0f);
        REQUIRE(buf1[2] == 1.0f);
        REQUIRE(buf1[3] == 2.0f);

        // Test phase shift larger than buffer size
        beforeEach();
        buf1.withPhase(5, work);  // Should be equivalent to phase=1
        REQUIRE(buf1[0] == 2.0f);
        REQUIRE(buf1[1] == 3.0f);
        REQUIRE(buf1[2] == 4.0f);
        REQUIRE(buf1[3] == 1.0f);
    }

    SECTION("sort") {
        std::array<float, 4> unsorted = {4.0f, 1.0f, 3.0f, 2.0f};
        buf1 = Buffer(unsorted.data(), 4);

        buf1.sort();
        REQUIRE(buf1[0] == 1.0f);
        REQUIRE(buf1[1] == 2.0f);
        REQUIRE(buf1[2] == 3.0f);
        REQUIRE(buf1[3] == 4.0f);
    }

    SECTION("dot product") {
        beforeEach();

        float dotProduct = buf1.dot(buf2);
        float expected = 1.0f * 0.5f + 2.0f * 1.5f + 3.0f * 2.5f + 4.0f * 3.5f;
        REQUIRE(dotProduct == Approx(expected));
    }

    SECTION("getMin/getMax") {
        beforeEach();

        float minVal = 0.0f;
        float maxVal = 0.0f;
        int minIdx = -1;
        int maxIdx = -1;

        buf1.getMin(minVal, minIdx);
        buf1.getMax(maxVal, maxIdx);

        REQUIRE(minVal == 1.0f);
        REQUIRE(minIdx == 0);
        REQUIRE(maxVal == 4.0f);
        REQUIRE(maxIdx == 3);

        // Test with unsorted data
        std::array<float, 4> unsorted = {3.0f, 1.0f, 4.0f, 2.0f};
        buf1 = Buffer(unsorted.data(), 4);

        buf1.getMin(minVal, minIdx);
        buf1.getMax(maxVal, maxIdx);

        REQUIRE(minVal == 1.0f);
        REQUIRE(minIdx == 1);
        REQUIRE(maxVal == 4.0f);
        REQUIRE(maxIdx == 2);
    }
}

TEST_CASE("Buffer transcendental operations", "[buffer][math]") {
    Buffer<float> buf;
    std::array<float, 4> data{};

    auto beforeEach = [&]() {
        data = {0.0f, 0.5f, 1.0f, 2.0f};
        buf = Buffer(data.data(), 4);
    };

    SECTION("natural logarithm") {
        beforeEach();

        buf.ln();
        REQUIRE(std::isinf(buf[0]));  // ln(0) = -inf
        REQUIRE(buf[1] == Approx(std::log(0.5f)));
        REQUIRE(buf[2] == Approx(0.0f));  // ln(1) = 0
        REQUIRE(buf[3] == Approx(std::log(2.0f)));
    }

    SECTION("sine") {
        beforeEach();

        buf.sin();
        REQUIRE(buf[0] == Approx(0.0f).margin(1e-6));
        REQUIRE(buf[1] == Approx(std::sin(0.5f)).margin(1e-6));
        REQUIRE(buf[2] == Approx(std::sin(1.0f)).margin(1e-6));
        REQUIRE(buf[3] == Approx(std::sin(2.0f)).margin(1e-6));
    }

    SECTION("exponential") {
        beforeEach();

        buf.exp();
        REQUIRE(buf[0] == Approx(1.0f));  // e^0 = 1
        REQUIRE(buf[1] == Approx(std::exp(0.5f)).margin(1e-6));
        REQUIRE(buf[2] == Approx(std::exp(1.0f)).margin(1e-6));  // e^1 = e
        REQUIRE(buf[3] == Approx(std::exp(2.0f)).margin(1e-6));
    }

    SECTION("hyperbolic tangent") {
        beforeEach();

        buf.tanh();
        REQUIRE(buf[0] == Approx(0.0f));  // tanh(0) = 0
        REQUIRE(buf[1] == Approx(std::tanh(0.5f)).margin(1e-6));
        REQUIRE(buf[2] == Approx(std::tanh(1.0f)).margin(1e-6));
        REQUIRE(buf[3] == Approx(std::tanh(2.0f)).margin(1e-6));

        // Test saturation
        data = {10.0f, -10.0f, 20.0f, -20.0f};
        buf = Buffer(data.data(), 4);
        buf.tanh();

        REQUIRE(buf[0] == Approx(1.0f).margin(1e-6));   // Should saturate to 1
        REQUIRE(buf[1] == Approx(-1.0f).margin(1e-6));  // Should saturate to -1
        REQUIRE(buf[2] == Approx(1.0f).margin(1e-6));   // Should saturate to 1
        REQUIRE(buf[3] == Approx(-1.0f).margin(1e-6));  // Should saturate to -1
    }
}

TEST_CASE("Buffer upsample operations", "[buffer][math]") {
    Buffer<float> buf;
    std::array<float, 4> data{};

    auto beforeEach = [&]() {
        data = {0.0f, 0.5f, 1.0f, 2.0f};
        buf = Buffer(data.data(), 4);
    };

    SECTION("upsample") {

    }

    SECTION("downsample") {

    }
}

TEST_CASE("Buffer downsampling operations", "[buffer][resample]") {
    Buffer<float> buf1, buf2;
    std::array<float, 8> data1{};
    std::array<float, 8> data2{};

    auto beforeEach = [&] {
        // Initialize with a simple counting sequence
        data1 = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        data2 = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        buf1 = Buffer(data1.data(), 8);
        buf2 = Buffer(data2.data(), 8);
    };

    SECTION("downsample by 2 with phase 0") {
        beforeEach();

        int resultPhase = buf2.downsampleFrom(buf1, 2, 0);

        // For factor 2, phase 0:
        // Output length should be (8 + 2 - 1 - 0)/2 = 4
        // Output should be [1, 3, 5, 7] (every 2nd sample starting at 0)
        REQUIRE(buf2[0] == 1.0f);
        REQUIRE(buf2[1] == 3.0f);
        REQUIRE(buf2[2] == 5.0f);
        REQUIRE(buf2[3] == 7.0f);

        // Phase calculation: (2 + 0 - 8%2)%2 = 0
        REQUIRE(resultPhase == 0);
    }

    SECTION("downsample by 2 with phase 1") {
        beforeEach();

        int resultPhase = buf2.downsampleFrom(buf1, 2, 1);

        // For factor 2, phase 1:
        // Output length should be (8 + 2 - 1 - 1)/2 = 4
        // Output should be [2, 4, 6, 8] (every 2nd sample starting at 1)
        REQUIRE(buf2[0] == 2.0f);
        REQUIRE(buf2[1] == 4.0f);
        REQUIRE(buf2[2] == 6.0f);
        REQUIRE(buf2[3] == 8.0f);

        // Phase calculation: (2 + 1 - 8%2)%2 = 1
        REQUIRE(resultPhase == 1);
    }

    SECTION("downsample by 3") {
        beforeEach();

        int resultPhase = buf2.downsampleFrom(buf1, 3, 0);

        // For factor 3, phase 0:
        // Output length should be (8 + 3 - 1 - 0)/3 = 3
        // Output should be [1, 4, 7] (every 3rd sample)
        REQUIRE(buf2[0] == 1.0f);
        REQUIRE(buf2[1] == 4.0f);
        REQUIRE(buf2[2] == 7.0f);

        // Phase calculation: (3 + 0 - 8%3)%3 = 1
        REQUIRE(resultPhase == 1);
    }
}

TEST_CASE("Buffer upsampling operations", "[buffer][resample]") {
    Buffer<float> buf1, buf2;
    std::array<float, 4> data1{};
    std::array<float, 12> data2{};

    auto beforeEach = [&]() {
        data1 = {1.0f, 2.0f, 3.0f, 4.0f};
        data2.fill(0.0f);
        buf1 = Buffer(data1.data(), 4);
        buf2 = Buffer(data2.data(), 12);
    };

    SECTION("upsample by 2 with phase 0") {
        beforeEach();

        int resultPhase = buf2.upsampleFrom(buf1, 2, 0);

        // For factor 2, phase 0:
        // Output length should be 2 * 4 = 8
        // Output should be [1,0, 2,0, 3,0, 4,0]
        REQUIRE(buf2[0] == 1.0f);
        REQUIRE(buf2[1] == 0.0f);
        REQUIRE(buf2[2] == 2.0f);
        REQUIRE(buf2[3] == 0.0f);
        REQUIRE(buf2[4] == 3.0f);
        REQUIRE(buf2[5] == 0.0f);
        REQUIRE(buf2[6] == 4.0f);
        REQUIRE(buf2[7] == 0.0f);

        REQUIRE(resultPhase == 0);
    }

    SECTION("upsample by 2 with phase 1") {
        beforeEach();

        int resultPhase = buf2.upsampleFrom(buf1, 2, 1);

        // For factor 2, phase 1:
        // Output length should be 2 * 4 = 8
        // Output should be [0,1, 0,2, 0,3, 0,4]
        REQUIRE(buf2[0] == 0.0f);
        REQUIRE(buf2[1] == 1.0f);
        REQUIRE(buf2[2] == 0.0f);
        REQUIRE(buf2[3] == 2.0f);
        REQUIRE(buf2[4] == 0.0f);
        REQUIRE(buf2[5] == 3.0f);
        REQUIRE(buf2[6] == 0.0f);
        REQUIRE(buf2[7] == 4.0f);

        REQUIRE(resultPhase == 1);
    }

    SECTION("upsample by 3") {
        beforeEach();

        int resultPhase = buf2.upsampleFrom(buf1, 3, 0);

        // For factor 3, phase 0:
        // Output length should be 3 * 4 = 12
        // Output should be [1,0,0, 2,0,0, 3,0,0, 4,0,0]
        REQUIRE(buf2[0] == 1.0f);
        REQUIRE(buf2[1] == 0.0f);
        REQUIRE(buf2[2] == 0.0f);
        REQUIRE(buf2[3] == 2.0f);
        REQUIRE(buf2[4] == 0.0f);
        REQUIRE(buf2[5] == 0.0f);
        REQUIRE(buf2[6] == 3.0f);
        REQUIRE(buf2[7] == 0.0f);
        REQUIRE(buf2[8] == 0.0f);
        REQUIRE(buf2[9] == 4.0f);
        REQUIRE(buf2[10] == 0.0f);
        REQUIRE(buf2[11] == 0.0f);

        REQUIRE(resultPhase == 0);
    }

    SECTION("edge case - default factor") {
        beforeEach();

        // When factor is -1, it should be calculated from buffer sizes
        // In this case, factor should be 3 (12/4)
        int resultPhase = buf2.upsampleFrom(buf1);

        REQUIRE(buf2[0] == 1.0f);
        REQUIRE(buf2[1] == 0.0f);
        REQUIRE(buf2[2] == 0.0f);
        REQUIRE(buf2[3] == 2.0f);
        REQUIRE(buf2[4] == 0.0f);
        REQUIRE(buf2[5] == 0.0f);
        REQUIRE(buf2[6] == 3.0f);
        REQUIRE(buf2[7] == 0.0f);
        REQUIRE(buf2[8] == 0.0f);
        REQUIRE(buf2[9] == 4.0f);
        REQUIRE(buf2[10] == 0.0f);
        REQUIRE(buf2[11] == 0.0f);
    }
}

// --------------- Complex arithmetic ---------------- //

TEST_CASE("Buffer complex arithmetic operations", "[buffer][complex]") {
    Buffer<Complex32> buf1, buf2;
    std::array<Complex32, 4> data1{};
    std::array<Complex32, 4> data2{};

    auto beforeEach = [&] {
        // Initialize with some basic complex numbers:
        // data1: {1+0i, 0+1i, -1+0i, 0-1i}
        // data2: {1+1i, -1+1i, -1-1i, 1-1i}
        data1 = {
            Complex32{1.0f, 0.0f},
            Complex32{0.0f, 1.0f},
            Complex32{-1.0f, 0.0f},
            Complex32{0.0f, -1.0f}
        };
        data2 = {
            Complex32{1.0f, 1.0f},
            Complex32{-1.0f, 1.0f},
            Complex32{-1.0f, -1.0f},
            Complex32{1.0f, -1.0f}
        };
        buf1 = Buffer(data1.data(), 4);
        buf2 = Buffer(data2.data(), 4);
    };

    SECTION("scalar multiplication") {
        beforeEach();
        Complex32 scalar{2.0f, 1.0f};  // 2 + i

        buf1.mul(scalar);

        // (1+0i)(2+i) = 2+i
        REQUIRE(real(buf1[0]) == Approx(2.0f));
        REQUIRE(imag(buf1[0]) == Approx(1.0f));

        // (0+i)(2+i) = -1+2i
        REQUIRE(real(buf1[1]) == Approx(-1.0f));
        REQUIRE(imag(buf1[1]) == Approx(2.0f));

        // (-1+0i)(2+i) = -2-i
        REQUIRE(real(buf1[2]) == Approx(-2.0f));
        REQUIRE(imag(buf1[2]) == Approx(-1.0f));

        // (0-i)(2+i) = 1-2i
        REQUIRE(real(buf1[3]) == Approx(1.0f));
        REQUIRE(imag(buf1[3]) == Approx(-2.0f));
    }

    SECTION("buffer addition") {
        beforeEach();

        buf1.add(buf2);

        // (1+0i) + (1+i) = 2+i
        REQUIRE(real(buf1[0]) == Approx(2.0f));
        REQUIRE(imag(buf1[0]) == Approx(1.0f));

        // (0+i) + (-1+i) = -1+2i
        REQUIRE(real(buf1[1]) == Approx(-1.0f));
        REQUIRE(imag(buf1[1]) == Approx(2.0f));

        // (-1+0i) + (-1-i) = -2-i
        REQUIRE(real(buf1[2]) == Approx(-2.0f));
        REQUIRE(imag(buf1[2]) == Approx(-1.0f));

        // (0-i) + (1-i) = 1-2i
        REQUIRE(real(buf1[3]) == Approx(1.0f));
        REQUIRE(imag(buf1[3]) == Approx(-2.0f));
    }

    SECTION("buffer subtraction") {
        beforeEach();

        buf1.sub(buf2);

        // (1+0i) - (1+i) = 0-i
        REQUIRE(real(buf1[0]) == Approx(0.0f));
        REQUIRE(imag(buf1[0]) == Approx(-1.0f));

        // (0+i) - (-1+i) = 1+0i
        REQUIRE(real(buf1[1]) == Approx(1.0f));
        REQUIRE(imag(buf1[1]) == Approx(0.0f));

        // (-1+0i) - (-1-i) = 0+i
        REQUIRE(real(buf1[2]) == Approx(0.0f));
        REQUIRE(imag(buf1[2]) == Approx(1.0f));

        // (0-i) - (1-i) = -1+0i
        REQUIRE(real(buf1[3]) == Approx(-1.0f));
        REQUIRE(imag(buf1[3]) == Approx(0.0f));
    }

    SECTION("buffer multiplication") {
        beforeEach();

        buf1.mul(buf2);

        // (1+0i)(1+i) = 1+i
        REQUIRE(real(buf1[0]) == Approx(1.0f));
        REQUIRE(imag(buf1[0]) == Approx(1.0f));

        // (0+i)(-1+i) = -1
        REQUIRE(real(buf1[1]) == Approx(-1.0f));
        REQUIRE(imag(buf1[1]) == Approx(-1.0f));

        // (-1+0i)(-1-i) = 1+i
        REQUIRE(real(buf1[2]) == Approx(1.0f));
        REQUIRE(imag(buf1[2]) == Approx(1.0f));

        // (0-i)(1-i) = -1 - i
        REQUIRE(real(buf1[3]) == Approx(-1.0f));
        REQUIRE(imag(buf1[3]) == Approx(-1.0f));
    }

    SECTION("buffer division") {
        beforeEach();

        buf1.div(buf2);

        // (1+0i)/(1+i) = (1-i)/2
        REQUIRE(real(buf1[0]) == Approx(0.5f));
        REQUIRE(imag(buf1[0]) == Approx(-0.5f));

        // (0+i)/(-1+i) = (-1-1)/2
        REQUIRE(real(buf1[1]) == Approx(0.5f));
        REQUIRE(imag(buf1[1]) == Approx(-0.5f));

        // (-1+0i)/(-1-i) = (1-i)/2
        REQUIRE(real(buf1[2]) == Approx(0.5f));
        REQUIRE(imag(buf1[2]) == Approx(-0.5f));

        // (0-i)/(1-i) = 0.5 - 0.5i
        REQUIRE(real(buf1[3]) == Approx(0.5f));
        REQUIRE(imag(buf1[3]) == Approx(-0.5f));
    }

    SECTION("addProduct with scalar") {
        beforeEach();
        Complex32 scalar{0.0f, 1.0f};  // i

        buf1.addProduct(buf2, scalar);

        // (1+0i) + (1+i)(i) = 1+0i + (-1+i) = 0+i
        REQUIRE(real(buf1[0]) == Approx(0.0f));
        REQUIRE(imag(buf1[0]) == Approx(1.0f));

        // (0+i) + (-1+i)(i) = i + (-i-1) = -1
        REQUIRE(real(buf1[1]) == Approx(-1.0f));
        REQUIRE(imag(buf1[1]) == Approx(0.0f));

        // (-1+0i) + (-1-i)(i) = -i
        REQUIRE(real(buf1[2]) == Approx(0.0f));
        REQUIRE(imag(buf1[2]) == Approx(-1.0f));

        // (0-i) + (1-i)(i) = 1
        REQUIRE(real(buf1[3]) == Approx(1.0f));
        REQUIRE(imag(buf1[3]) == Approx(0.0f));
    }

    SECTION("addProduct with two buffers") {
        beforeEach();
        std::array<Complex32, 4> data3 = {
            Complex32{1.0f, 0.0f},
            Complex32{1.0f, 0.0f},
            Complex32{1.0f, 0.0f},
            Complex32{1.0f, 0.0f}
        };
        Buffer result(data3.data(), 4);

        result.addProduct(buf1, buf2);

        // 1 + (1+0i)(1+i) = 1 + (1+i) = 2+i
        REQUIRE(real(result[0]) == Approx(2.0f));
        REQUIRE(imag(result[0]) == Approx(1.0f));

        // 1 + (0+i)(-1+i) = 1 + (-1) = 0
        REQUIRE(real(result[1]) == Approx(0.0f));
        REQUIRE(imag(result[1]) == Approx(-1.0f));

        // 1 + (-1+0i)(-1-i) = 1 + (1+i) = 2+i
        REQUIRE(real(result[2]) == Approx(2.0f));
        REQUIRE(imag(result[2]) == Approx(1.0f));

        // 1 + (0-i)(1-i) = -i
        REQUIRE(real(result[3]) == Approx(0.0f));
        REQUIRE(imag(result[3]) == Approx(-1.0f));
    }
}