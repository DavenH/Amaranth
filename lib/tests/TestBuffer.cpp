#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Array/Buffer.h"
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
    std::array<float, 4> data1 = {1.0f, 2.0f, 3.0f, 4.0f};
    std::array<float, 4> data2 = {2.0f, 3.0f, 4.0f, 5.0f};
    
    SECTION("add") {
        Buffer<float> buf1(data1.data(), 4);
        Buffer<float> buf2(data2.data(), 4);
        buf1.add(buf2);
        REQUIRE(buf1[0] == 3.0f);
        REQUIRE(buf1[3] == 9.0f);
    }
    
    SECTION("mul") {
        Buffer<float> buf1(data1.data(), 4);
        Buffer<float> buf2(data2.data(), 4);
        buf1.mul(buf2);
        REQUIRE(buf1[0] == 2.0f);
        REQUIRE(buf1[3] == 20.0f);
    }
    
    SECTION("scalar operations") {
        Buffer<float> buf(data1.data(), 4);
        buf.mul(2.0f);
        REQUIRE(buf[0] == 2.0f);
        REQUIRE(buf[3] == 8.0f);
        
        buf.add(1.0f);
        REQUIRE(buf[0] == 3.0f);
        REQUIRE(buf[3] == 9.0f);
    }
}

TEST_CASE("Buffer statistical operations", "[buffer]") {
    std::array<float, 4> data = {1.0f, 2.0f, 3.0f, 4.0f};
    Buffer<float> buf(data.data(), 4);
    
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
        REQUIRE(buf.stddev() == Approx(1.2909944f));
    }
}

TEST_CASE("Buffer threshold operations", "[buffer]") {
    std::array<float, 4> data = {-1.0f, 0.5f, 2.0f, 3.5f};
    
    SECTION("clip") {
        Buffer<float> buf(data.data(), 4);
        buf.clip(0.0f, 2.0f);
        REQUIRE(buf[0] == 0.0f);
        REQUIRE(buf[1] == 0.5f);
        REQUIRE(buf[2] == 2.0f);
        REQUIRE(buf[3] == 2.0f);
    }
    
    SECTION("threshold operations") {
        Buffer<float> buf(data.data(), 4);
        buf.threshLT(1.0f);
        REQUIRE(buf[0] == -1.0f);
        REQUIRE(buf[1] == 0.5f);
        REQUIRE(buf[2] == 1.0f);
        REQUIRE(buf[3] == 1.0f);
        
        buf = Buffer<float>(data.data(), 4);
        buf.threshGT(1.0f);
        REQUIRE(buf[0] == 1.0f);
        REQUIRE(buf[1] == 1.0f);
        REQUIRE(buf[2] == 2.0f);
        REQUIRE(buf[3] == 3.5f);
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
        buf.div(0.0f); // Should be a no-op according to comments
        REQUIRE(buf[0] == 1.0f);
        REQUIRE(buf[3] == 4.0f);
    }
}

TEST_CASE("Buffer arithmetic operators", "[buffer][arithmetic]") {
    std::array<float, 4> data1 = {1.0f, 2.0f, 3.0f, 4.0f};
    std::array<float, 4> data2 = {0.5f, 1.5f, 2.5f, 3.5f};

    SECTION("operator+=") {
        Buffer<float> buf1(data1.data(), 4);
        Buffer<float> buf2(data2.data(), 4);

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
        Buffer<float> buf1(data1.data(), 4);
        Buffer<float> buf2(data2.data(), 4);

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
        Buffer<float> buf1(data1.data(), 4);
        Buffer<float> buf2(data2.data(), 4);

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
        Buffer<float> buf1(data1.data(), 4);
        Buffer<float> buf2(data2.data(), 4);

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
    std::array<float, 4> data1 = {1.0f, 2.0f, 3.0f, 4.0f};
    std::array<float, 4> data2 = {0.5f, 1.5f, 2.5f, 3.5f};

    SECTION("addProduct with scalar") {
        Buffer<float> buf1(data1.data(), 4);
        Buffer<float> buf2(data2.data(), 4);

        buf1.addProduct(buf2, 2.0f);  // buf1[i] += buf2[i] * 2.0f
        REQUIRE(buf1[0] == 2.0f);     // 1.0 + (0.5 * 2.0)
        REQUIRE(buf1[1] == 5.0f);     // 2.0 + (1.5 * 2.0)
        REQUIRE(buf1[2] == 8.0f);     // 3.0 + (2.5 * 2.0)
        REQUIRE(buf1[3] == 11.0f);    // 4.0 + (3.5 * 2.0)
    }

    SECTION("addProduct with two buffers") {
        std::array<float, 4> data3 = {1.0f, 1.0f, 1.0f, 1.0f};
        Buffer<float> result(data3.data(), 4);
        Buffer<float> buf1(data1.data(), 4);
        Buffer<float> buf2(data2.data(), 4);

        result.addProduct(buf1, buf2);  // result[i] += buf1[i] * buf2[i]
        REQUIRE(result[0] == 1.5f);     // 1.0 + (1.0 * 0.5)
        REQUIRE(result[1] == 4.0f);     // 1.0 + (2.0 * 1.5)
        REQUIRE(result[2] == 8.5f);     // 1.0 + (3.0 * 2.5)
        REQUIRE(result[3] == 15.0f);    // 1.0 + (4.0 * 3.5)
    }

    SECTION("reverse operations") {
        Buffer<float> buf(data1.data(), 4);

        buf.subCRev(5.0f);  // buf[i] = 5.0f - buf[i]
        REQUIRE(buf[0] == 4.0f);
        REQUIRE(buf[1] == 3.0f);
        REQUIRE(buf[2] == 2.0f);
        REQUIRE(buf[3] == 1.0f);

        buf = Buffer<float>(data1.data(), 4);
        buf.divCRev(12.0f);  // buf[i] = 12.0f / buf[i]
        REQUIRE(buf[0] == 12.0f);
        REQUIRE(buf[1] == 6.0f);
        REQUIRE(buf[2] == 4.0f);
        REQUIRE(buf[3] == 3.0f);

        buf = Buffer<float>(data1.data(), 4);
        buf.powCRev(2.0f);  // buf[i] = 2.0f ** buf[i]
        REQUIRE(buf[0] == 2.0f);
        REQUIRE(buf[1] == 4.0f);
        REQUIRE(buf[2] == 8.0f);
        REQUIRE(buf[3] == 16.0f);
    }
}

TEST_CASE("Buffer arithmetic edge cases", "[buffer][arithmetic]") {
    SECTION("division by zero") {
        std::array<float, 4> data = {1.0f, 2.0f, 3.0f, 4.0f};
        Buffer<float> buf(data.data(), 4);
        Buffer<float> zero(data.data(), 4);
        zero.zero();

        // Test division by zero buffer
        buf /= zero;  // Should be a no-op
        REQUIRE(buf[0] == 1.0f);
        REQUIRE(buf[1] == 2.0f);
        REQUIRE(buf[2] == 3.0f);
        REQUIRE(buf[3] == 4.0f);

        // Test scalar division by zero
        buf /= 0.0f;  // Should be a no-op
        REQUIRE(buf[0] == 1.0f);
        REQUIRE(buf[1] == 2.0f);
        REQUIRE(buf[2] == 3.0f);
        REQUIRE(buf[3] == 4.0f);
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