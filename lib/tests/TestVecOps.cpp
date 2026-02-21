#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Array/VecOps.h"
#include "TestDefs.h"

using namespace VecOps;

TEST_CASE("VecOps::zero", "[vecops]") {
    SECTION("Float32 array zeroing") {
        const int size = 100;
        auto* data = allocate<Float32>(size);

        // Fill with non-zero values
        for (int i = 0; i < size; ++i) {
            data[i] = static_cast<Float32>(i + 1);
        }

        zero(data, size);

        for (int i = 0; i < size; ++i) {
            REQUIRE(data[i] == 0.0f);
        }

        deallocate(data);
    }

    SECTION("Float64 array zeroing") {
        const int size = 100;
        auto* data = allocate<Float64>(size);

        // Fill with non-zero values
        for (int i = 0; i < size; ++i) {
            data[i] = static_cast<Float64>(i + 1);
        }

        zero(data, size);

        for (int i = 0; i < size; ++i) {
            REQUIRE(data[i] == 0.0);
        }

        deallocate(data);
    }

    SECTION("Zero-length array") {
        Float32* data = nullptr;
        REQUIRE_NOTHROW(zero(data, 0));
    }
}

TEST_CASE("VecOps::copy", "[vecops]") {
    SECTION("Float32 array copying") {
        const int size = 100;
        auto* src = allocate<Float32>(size);
        auto* dst = allocate<Float32>(size);

        for (int i = 0; i < size; ++i) {
            src[i] = static_cast<Float32>(i);
        }

        copy(src, dst, size);

        for (int i = 0; i < size; ++i) {
            REQUIRE(dst[i] == src[i]);
        }

        deallocate(src);
        deallocate(dst);
    }

    SECTION("Int16s array copying") {
        const int size = 100;
        auto* src = allocate<Int16s>(size);
        auto* dst = allocate<Int16s>(size);

        for (int i = 0; i < size; ++i) {
            src[i] = static_cast<Int16s>(i);
        }

        copy(src, dst, size);

        for (int i = 0; i < size; ++i) {
            REQUIRE(dst[i] == src[i]);
        }

        deallocate(src);
        deallocate(dst);
    }

    SECTION("Zero-length array") {
        Float32* src = nullptr;
        Float32* dst = nullptr;
        REQUIRE_NOTHROW(copy(src, dst, 0));
    }
}

TEST_CASE("VecOps buffer operations", "[vecops]") {
    const int size = 100;

    Float32 *dataA, *dataB, *result;
    Buffer<float> srcA, srcB, dst;

    auto beforeEach = [&](
        std::function<int(int)> setter1,
        std::function<int(int)> setter2) {
        dataA = allocate<Float32>(size);
        dataB = allocate<Float32>(size);
        result = allocate<Float32>(size);

        for (int i = 0; i < size; ++i) {
            dataA[i] = (float) setter1(i);
            dataB[i] = (float) setter2(i);
        }

        srcA = Buffer(dataA, size);
        srcB = Buffer(dataB, size);
        dst = Buffer(result, size);
    };

    auto afterEach = [&]() {
        deallocate(dataA);
        deallocate(dataB);
        deallocate(result);
    };

    SECTION("Buffer addition") {
        beforeEach(
            [](int i) { return i; },
            [](int i) { return i * 2; }
        );
        add(srcA, srcB, dst);

        for (int i = 0; i < dst.size(); ++i) {
            REQUIRE(dst[i] == Catch::Approx(i + i * 2));
        }

        afterEach();
    }

    SECTION("Buffer subtraction") {
        beforeEach(
            [](int i) { return i * 3 ; },
            [](int i) { return i; }
        );

        sub(srcA, srcB, dst);

        for (int i = 0; i < dst.size(); ++i) {
            REQUIRE(dst[i] == Catch::Approx(i * 2));
        }

        afterEach();
    }

    SECTION("Buffer multiplication") {
        beforeEach(
            [](int i) { return i + 1; },
            [](int i) { return 2; }
        );

        mul(srcA, srcB, dst);

        for (int i = 0; i < dst.size(); ++i) {
            REQUIRE(dst[i] == Catch::Approx((i + 1) * 2));
        }

        afterEach();
    }

    SECTION("Buffer division") {
        beforeEach(
            [](int i) { return (i + 1) * 2; },
            [](int i) { return 2; }
        );

        div(srcA, srcB, dst);

        for (int i = 0; i < dst.size(); ++i) {
            REQUIRE(dst[i] == Catch::Approx(i + 1));
        }

        afterEach();
    }

    SECTION("Division by zero handling") {
        beforeEach(
            [](int i) { return 1; },
            [](int i) { return 0; }
        );

        // Should not crash, but results may be undefined
        REQUIRE_NOTHROW(div(srcA, srcB, dst));
        afterEach();
    }
}

TEST_CASE("VecOps buffer K operations", "[vecops]") {
    // dst[i] = k / src[i]
    // T = Float32
    // template<typename T> void divCRev(Buffer<T> src, T k, Buffer<T> dst);

    // dst[i] = k * src[i]
    // T = Float32, Float64
    // template<typename T> void mul(Buffer<T> srcA, T k, Buffer<T> dst);

    // T = Float32
    // template<typename T> void mul(T* srcA, T k, T* dst, int len);

    // T = Float32
    // template<typename T> void addProd(T* srcA, T k, T* dst, int len);

    Float32 *data, *result;
    Buffer<float> src, dst;

    auto beforeEach = [&](
        std::function<int(int)> setter1, int size = 10) {
        data = allocate<Float32>(size);
        result = allocate<Float32>(size);

        for (int i = 0; i < size; ++i) {
            data[i] = (float) setter1(i);
        }

        src = Buffer(data, size);
        dst = Buffer(result, size);
    };

    auto afterEach = [&] {
        deallocate(data);
        deallocate(result);
    };

    SECTION("Buffer reverse div C") {
        beforeEach(
            [](int i) { return (i + 1); }
        );

        divCRev(src, 10.f, dst);
        for (int i = 0; i < 10; ++i) {
            REQUIRE(result[i] == Catch::Approx(10.f / (i + 1)));
        }

        afterEach();
    }

    SECTION("Buffer mul C") {
        beforeEach(
            [](int i) { return (i + 1); }
        );

        mul(src, 10.f, dst);
        for (int i = 0; i < 10; ++i) {
            REQUIRE(result[i] == Catch::Approx(10.f * (i + 1)));
        }

        afterEach();
    }

    SECTION("Float* mul C") {
        beforeEach(
            [](int i) { return (i + 1); }
        );

        mul(src.get(), 10.f, dst.get(), dst.size());
        for (int i = 0; i < 10; ++i) {
            REQUIRE(result[i] == Catch::Approx(10.f * (i + 1)));
        }

        afterEach();
    }

    SECTION("Float* add Product C") {
        beforeEach(
            [](int i) { return (i + 1); }
        );

        for (int i = 0; i < 10; ++i) {
            dst[i] = 1.0;
        }

        addProd(src.get(), 10.f, dst.get(), dst.size());

        for (int i = 0; i < 10; ++i) {
            REQUIRE(result[i] == Catch::Approx(1.0 + 10.f * (i + 1)));
        }

        afterEach();
    }
}

TEST_CASE("VecOps Complex32 operations", "[vecops]") {
    const int size = 50;

    Complex32 *dataA, *dataB, *result;
    Buffer<Complex32> srcA, srcB, dst;

    auto beforeEach = [&](
        std::function<Complex32(int)> setter1,
        std::function<Complex32(int)> setter2) {
        dataA = allocate<Complex32>(size);
        dataB = allocate<Complex32>(size);
        result = allocate<Complex32>(size);

        for (int i = 0; i < size; ++i) {
            dataA[i] = setter1(i);
            dataB[i] = setter2(i);
        }

        srcA = Buffer(dataA, size);
        srcB = Buffer(dataB, size);
        dst = Buffer(result, size);
    };

    auto afterEach = [&]() {
        deallocate(dataA);
        deallocate(dataB);
        deallocate(result);
    };

    SECTION("Complex32 addition") {
        beforeEach(
            [](int i) { return makeComplex((float)i, (float)i); },
            [](int i) { return makeComplex(1.0f, 2.0f); }
        );

        add(srcA, srcB, dst);

        for (int i = 0; i < size; ++i) {
            REQUIRE(real(dst[i]) == Catch::Approx(i + 1.0f));
            REQUIRE(imag(dst[i]) == Catch::Approx(i + 2.0f));
        }

        afterEach();
    }

    SECTION("Complex32 subtraction") {
        beforeEach(
            [](int i) { return makeComplex((float)i * 2.0f, (float)i * 3.0f); },
            [](int i) { return makeComplex((float)i, (float)i); }
        );

        sub(srcA, srcB, dst);

        for (int i = 0; i < dst.size(); ++i) {
            REQUIRE(real(dst[i]) == Catch::Approx(i));
            REQUIRE(imag(dst[i]) == Catch::Approx(i * 2.0f));
        }

        afterEach();
    }

    SECTION("Complex32 multiplication") {
        beforeEach(
            [](int i) { return makeComplex(2.0f, 1.0f); },
            [](int i) { return makeComplex(3.0f, 4.0f); }
        );

        mul(srcA, srcB, dst);

        // For complex multiplication (a + bi)(c + di) = (ac - bd) + (ad + bc)i
        for (int i = 0; i < size; ++i) {
            // (2 + i)(3 + 4i) = (2*3 - 1*4) + (2*4 + 1*3)i = 2 + 11i
            REQUIRE(real(dst[i]) == Catch::Approx(2.0f));
            REQUIRE(imag(dst[i]) == Catch::Approx(11.0f));
        }

        afterEach();
    }

    SECTION("Complex32 multiplication with i") {
        beforeEach(
            [](int i) { return makeComplex((float)i, 0.0f); },
            [](int i) { return makeComplex(0.0f, 1.0f); }  // i
        );

        mul(srcA, srcB, dst);

        // Multiplication by i rotates by 90 degrees
        for (int i = 0; i < size; ++i) {
            REQUIRE(real(dst[i]) == Catch::Approx(0.0f));
            REQUIRE(imag(dst[i]) == Catch::Approx((float)i));
        }

        afterEach();
    }

    SECTION("Complex32 division") {
        beforeEach(
            [](int i) { return makeComplex(5.0f, 10.0f); },
            [](int i) { return makeComplex(2.0f, -1.0f); }
        );

        div(srcA, srcB, dst);

        // For complex division (a + bi)/(c + di) = ((ac + bd)/(c² + d²)) + ((bc - ad)/(c² + d²))i
        for (int i = 0; i < size; ++i) {
            // (5 + 10i)/(2 - i) = ((5*2 + 10*(-1))/(2² + (-1)²)) + ((10*2 - 5*(-1))/(2² + (-1)²))i
            // = (10 - 10)/5 + (20 + 5)/5i = 0 + 5i
            REQUIRE(real(result[i]) == Catch::Approx(0.0f));
            REQUIRE(imag(result[i]) == Catch::Approx(5.0f));
        }

        afterEach();
    }

    SECTION("Complex32 division by zero") {
        beforeEach(
            [](int i) { return makeComplex(1.0f, 1.0f); },
            [](int i) { return makeComplex(0.0f, 0.0f); }
        );

        // Should not crash, but results may be undefined
        REQUIRE_NOTHROW(div(srcA, srcB, dst));

        afterEach();
    }

    SECTION("Complex32 division by pure imaginary") {
        beforeEach(
            [](int i) { return makeComplex(4.0f, 2.0f); },
            [](int i) { return makeComplex(0.0f, 2.0f); }
        );

        div(srcA, srcB, dst);

        // Division by i rotates by 90 degrees and scales
        for (int i = 0; i < size; ++i) {
            REQUIRE(real(result[i]) == Catch::Approx(1.0f).epsilon(1e-6));
            REQUIRE(imag(result[i]) == Catch::Approx(-2.0f).epsilon(1e-6));
        }

        afterEach();
    }
}

TEST_CASE("VecOps::move operations", "[vecops]") {
    const int size = 100;
    Float32 *dataA, *result;
    Buffer<float> srcA, dst;

    auto beforeEach = [&](std::function<int(int)> setter1) {
        dataA = allocate<Float32>(size);
        result = allocate<Float32>(size);

        for (int i = 0; i < size; ++i) {
            dataA[i] = (float)setter1(i);
            result[i] = 0.0f;  // Initialize destination to zeros
        }

        srcA = Buffer(dataA, size);
        dst = Buffer(result, size);
    };

    auto afterEach = [&]() {
        deallocate(dataA);
        deallocate(result);
    };

    SECTION("Non-overlapping move") {
        beforeEach([](int i) { return i + 1; });

        move(srcA, dst);

        for (int i = 0; i < size; ++i) {
            REQUIRE(dst[i] == Catch::Approx(i + 1));
        }

        afterEach();
    }

    SECTION("Overlapping move - forward direction") {
        beforeEach(
            [](int i) { return i + 1; }
        );

        // Create overlapping buffers with offset
        const int offset = 10;
        Buffer<Float32> overlapped = srcA + offset;

        move(srcA, overlapped);

        // Verify the moved data
        for (int i = 0; i < overlapped.size(); ++i) {
            REQUIRE(overlapped[i] == Catch::Approx(i + 1));
        }

        afterEach();
    }

    SECTION("Zero-length move") {
        auto* emptyData = allocate<Float32>(0);
        Buffer empty(emptyData, 0);

        REQUIRE_NOTHROW(move(empty, empty));

        deallocate(emptyData);
    }
}

TEST_CASE("VecOps::diff operations", "[vecops]") {
    Float32 *dataA, *result;
    Buffer<float> srcA, dst;

    auto beforeEach = [&](std::function<int(int)> setter1, int size = 100) {
        dataA = allocate<Float32>(size);
        result = allocate<Float32>(size);

        for (int i = 0; i < size; ++i) {
            dataA[i] = (float)setter1(i);
        }

        srcA = Buffer(dataA, size);
        dst = Buffer(result, size);
    };

    auto afterEach = [&]() {
        deallocate(dataA);
        deallocate(result);
    };

    SECTION("Linear sequence diff") {
        int size = 100;
        beforeEach([](int i) { return i * 2; }, size);  // 0, 2, 4, 6, ...

        diff(srcA, dst);

        // Verify differences
        for (int i = 0; i < size - 1; ++i) {
            REQUIRE(dst[i] == Catch::Approx(2.0f));  // Constant difference of 2
        }
        // Last element should equal second-to-last element
        REQUIRE(dst[size - 1] == Catch::Approx(2.0f));

        afterEach();
    }

    SECTION("Constant sequence diff") {
        beforeEach([](int i) { return 5; });

        diff(srcA, dst);

        for (int i = 0; i < 100; ++i) {
            REQUIRE(dst[i] == Catch::Approx(0.0f));
        }

        afterEach();
    }

    SECTION("Minimum size sequence (2 elements)") {
        int size = 2;
        beforeEach([](int i) { return i + 1; }, size);  // 1, 2, 4, 8, ...

        diff(srcA, dst);

        REQUIRE(dst[0] == Catch::Approx(1.0f));
        REQUIRE(dst[1] == Catch::Approx(1.0f));
        afterEach();
    }
}

TEST_CASE("VecOps::flip operations", "[vecops]") {
    const int size = 15;
    Float32 *dataA, *result;
    Buffer<float> srcA, srcB, dst;

    auto beforeEach = [&](std::function<int(int)> setter1) {
        dataA = allocate<Float32>(size);
        result = allocate<Float32>(size);

        for (int i = 0; i < size; ++i) {
            dataA[i] = (float)setter1(i);
        }

        srcA = Buffer(dataA, size);
        dst = Buffer(result, size);
    };

    auto afterEach = [&] {
        deallocate(dataA);
        deallocate(result);
    };

    SECTION("Flip ascending sequence") {
        beforeEach([](int i) { return i; });  // 0, 1, 2, 3, ...

        flip(srcA, dst);

        for (int i = 0; i < size; ++i) {
            REQUIRE(dst[i] == Catch::Approx(size - 1 - i));
        }

        afterEach();
    }

    SECTION("In-place flip") {
        beforeEach([](int i) { return i; });

        // Use same buffer for source and destination
        flip(srcA, srcA);

        for (int i = 0; i < size; ++i) {
            REQUIRE(srcA[i] == Catch::Approx(size - 1 - i));
        }

        afterEach();
    }

    SECTION("Single element sequence") {
        auto* singleData = allocate<Float32>(1);
        singleData[0] = 42.0f;
        Buffer singleBuff(singleData, 1);

        flip(singleBuff, singleBuff);

        REQUIRE(singleBuff[0] == Catch::Approx(42.0f));

        deallocate(singleData);
    }
}

TEST_CASE("VecOps::interleave operations", "[vecops]") {
    const int size = 50;
    Float32 *dataA, *dataB, *result;
    Buffer<Float32> srcA, srcB, dst;

    auto beforeEach = [&](
        std::function<float(int)> setter1,
        std::function<float(int)> setter2,
        int sz = 50) {
        dataA = allocate<Float32>(sz);
        dataB = allocate<Float32>(sz);
        result = allocate<Float32>(sz * 2);  // Destination needs double size

        for (int i = 0; i < sz; ++i) {
            dataA[i] = setter1(i);
            dataB[i] = setter2(i);
        }

        srcA = Buffer(dataA, sz);
        srcB = Buffer(dataB, sz);
        dst = Buffer(result, sz * 2);
    };

    auto afterEach = [&] {
        deallocate(dataA);
        deallocate(dataB);
        deallocate(result);
    };

    SECTION("Basic interleaving") {
        beforeEach(
            [](int i) { return static_cast<float>(i); },
            [](int i) { return static_cast<float>(i * 10); }
        );

        interleave(srcA, srcB, dst);

        for (int i = 0; i < size; ++i) {
            REQUIRE(result[i * 2] == Catch::Approx(i));
            REQUIRE(result[i * 2 + 1] == Catch::Approx(i * 10));
        }

        afterEach();
    }

    SECTION("Single element interleave") {
        beforeEach(
            [](int i) { return 1; },
            [](int i) { return 2; },
            1
        );

        interleave(srcA, srcB, dst);

        REQUIRE(dst[0] == Catch::Approx(1.0f));
        REQUIRE(dst[1] == Catch::Approx(2.0f));

        afterEach();
    }
}

template<class T, class S> std::pair<Buffer<T>, Buffer<S>> prepareBuffers(std::function<T(int)> setter1) {
    int size = 10;
    T* data = allocate<T>(size);
    S* result = allocate<S>(size);

    for (int i = 0; i < size; ++i) {
        data[i] = setter1(i);
    }

    return std::make_pair(Buffer(data, size), Buffer(result, size));
};

TEST_CASE("VecOps::roundDown operations", "[vecops]") {

    SECTION("Float32 to Int16s rounding") {
        auto pair = prepareBuffers<Float32, Int16s>(
            [](int i) { return static_cast<Float32>(i % 256) + 0.7f; }
        );

        roundDown(pair.first, pair.second);

        for (int i = 0; i < pair.first.size(); ++i) {
            REQUIRE(pair.second[i] == static_cast<Int16s>(i % 256));
        }

        deallocate(pair.first.get());
        deallocate(pair.second.get());
    }

    SECTION("Float32 to Int32s rounding") {
        auto pair = prepareBuffers<Float32, Int32s>(
            [](int i) { return static_cast<Float32>(i * 1000) + 0.7f; }
        );

        roundDown(pair.first, pair.second);
        int size = pair.first.size();

        for (int i = 0; i < size; ++i) {
            REQUIRE(pair.second[i] == static_cast<Int32s>(i * 1000));
        }

        deallocate(pair.first.get());
        deallocate(pair.second.get());
    }

    SECTION("Float64 to Int16s rounding") {
        auto pair = prepareBuffers<Float32, Int16s>(
            [](int i) { return static_cast<Float64>(i - 5) + 0.7; }
        );

        roundDown(pair.first, pair.second);

        for (int i = 0; i < pair.second.size(); ++i) {
            REQUIRE(pair.second[i] == (pair.first[i] < 0 ? static_cast<Int16s>(i - 4) : static_cast<Int16s>(i - 5)));
        }

        deallocate(pair.first.get());
        deallocate(pair.second.get());
    }
}

TEST_CASE("VecOps::convert operations", "[vecops]") {

    SECTION("Float32 to Float64 conversion") {
        auto pair = prepareBuffers<Float32, Float64>(
            [](int i) { return static_cast<Float32>(i) + 0.123f; }
        );
        convert(pair.first, pair.second);

        for (int i = 0; i < pair.second.size(); ++i) {
            // Allow small epsilon due to floating point conversion
            REQUIRE(pair.second[i] == Catch::Approx(static_cast<Float64>(pair.first[i])).epsilon(1e-6));
        }

        deallocate(pair.first.get());
        deallocate(pair.second.get());
    }

    SECTION("Float64 to Float32 conversion") {
        auto pair = prepareBuffers<Float64, Float32>(
            [](int i) { return static_cast<Float64>(i) * 0.01 + 0.123456789; }
        );
        convert(pair.first, pair.second);

        for (int i = 0; i < pair.second.size(); ++i) {
            // Allow small epsilon due to floating point conversion
            REQUIRE(pair.second[i] == Catch::Approx(static_cast<Float32>(pair.first[i])).epsilon(1e-5));

        }

        deallocate(pair.first.get());
        deallocate(pair.second.get());
    }
}