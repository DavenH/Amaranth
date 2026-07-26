#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

#include "../src/Array/ScopedAlloc.h"

static_assert(!std::is_copy_constructible_v<ScopedAlloc<float>>);
static_assert(!std::is_copy_assignable_v<ScopedAlloc<float>>);
static_assert(std::is_nothrow_move_constructible_v<ScopedAlloc<float>>);
static_assert(std::is_nothrow_move_assignable_v<ScopedAlloc<float>>);

TEST_CASE("ScopedAlloc basic construction and state", "[ScopedAlloc]") {
    SECTION("Default construction") {
        ScopedAlloc<float> alloc;
        CHECK(alloc.size() == 0);
        CHECK(alloc.get() == nullptr);
    }

    SECTION("Size construction") {
        ScopedAlloc<float> alloc(100);
        CHECK(alloc.size() == 100);
        CHECK(alloc.get() != nullptr);
    }

    SECTION("Zero size construction") {
        ScopedAlloc<float> alloc(0);
        CHECK(alloc.size() == 0);
        CHECK(alloc.get() == nullptr);
    }
}

TEST_CASE("ScopedAlloc memory management", "[ScopedAlloc]") {
    SECTION("Clear releases memory") {
        ScopedAlloc<float> alloc(100);
        alloc.clear();
        CHECK(alloc.size() == 0);
        CHECK(alloc.get() == nullptr);
    }

    SECTION("Resize behavior") {
        ScopedAlloc<float> alloc;
        REQUIRE(alloc.resize(50));
        CHECK(alloc.size() == 50);
        CHECK(alloc.get() != nullptr);

        REQUIRE(alloc.resize(100));
        CHECK(alloc.size() == 100);
        alloc.clear();
        REQUIRE(alloc.resize(75));
        CHECK(alloc.size() == 75);
    }

    SECTION("EnsureSize behavior") {
        ScopedAlloc<float> alloc(50);
        CHECK_FALSE(alloc.ensureSize(25));
        CHECK(alloc.size() == 50);

        CHECK(alloc.ensureSize(100));
        CHECK(alloc.size() == 100);
    }
}

TEST_CASE("ScopedAlloc placement allocation", "[ScopedAlloc]") {
    SECTION("Basic placement") {
        ScopedAlloc<float> alloc(100);
        Buffer<float> buff1 = alloc.place(30);
        CHECK(buff1.size() == 30);
        CHECK(buff1.get() == alloc.get());
        Buffer<float> buff2 = alloc.place(40);
        CHECK(buff2.size() == 40);
        CHECK(buff2.get() == alloc.get() + 30);
    }

    SECTION("Reset placement") {
        ScopedAlloc<float> alloc(100);
        Buffer<float> buff1 = alloc.place(30);
        alloc.resetPlacement();

        Buffer<float> buff2 = alloc.place(40);
        CHECK(buff2.get() == alloc.get());
    }

    SECTION("Placement bounds checking") {
        ScopedAlloc<float> alloc(100);
        alloc.place(60);
        CHECK(alloc.hasSizeFor(40));
        CHECK_FALSE(alloc.hasSizeFor(41));
    }
}

TEST_CASE("ScopedAlloc move semantics", "[ScopedAlloc]") {
    SECTION("Move construction transfers ownership and placement") {
        ScopedAlloc<float> source(100);
        source.place(30);
        float* originalPtr = source.get();

        ScopedAlloc<float> dest(std::move(source));

        CHECK(source.size() == 0);
        CHECK(source.get() == nullptr);
        CHECK(dest.size() == 100);
        CHECK(dest.get() == originalPtr);
        CHECK(dest.place(10).get() == originalPtr + 30);
    }

    SECTION("Move assignment replaces ownership") {
        ScopedAlloc<float> source(100);
        source.place(25);
        float* originalPtr = source.get();
        ScopedAlloc<float> dest(50);

        dest = std::move(source);

        CHECK(source.empty());
        CHECK(source.get() == nullptr);
        CHECK(dest.size() == 100);
        CHECK(dest.get() == originalPtr);
        CHECK(dest.place(10).get() == originalPtr + 25);
    }

    SECTION("Moved-from allocation can be reused") {
        ScopedAlloc<float> source(100);
        ScopedAlloc<float> dest(std::move(source));

        REQUIRE(source.resize(20));
        CHECK(source.size() == 20);
        CHECK(source.get() != nullptr);
        CHECK(dest.size() == 100);
    }

    SECTION("Self move assignment preserves ownership") {
        ScopedAlloc<float> alloc(100);
        float* originalPtr = alloc.get();
        int originalSize = alloc.size();

        alloc = std::move(alloc);

        CHECK(alloc.size() == originalSize);
        CHECK(alloc.get() == originalPtr);
    }

    SECTION("Compatibility ownership transfer delegates to move assignment") {
        ScopedAlloc<float> source(100);
        float* originalPtr = source.get();
        ScopedAlloc<float> dest;

        dest.takeOwnershipFrom(std::move(source));

        CHECK(source.empty());
        CHECK(dest.get() == originalPtr);
    }
}

TEST_CASE("ScopedAlloc with different types", "[ScopedAlloc]") {
    SECTION("Integer allocation") {
        ScopedAlloc<int> alloc(100);
        CHECK(alloc.size() == 100);
        Buffer<int> buff = alloc.place(50);
        CHECK(buff.size() == 50);
    }

    SECTION("Double allocation") {
        ScopedAlloc<double> alloc(100);
        CHECK(alloc.size() == 100);
        Buffer<double> buff = alloc.place(50);
        CHECK(buff.size() == 50);
    }
}
