#include <catch2/catch_test_macros.hpp>
#include "../src/Array/ScopedAlloc.h"

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

    SECTION("Copy construction is disabled") {
        ScopedAlloc<float> alloc(100);
        CHECK_THROWS(ScopedAlloc<float>(alloc));
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
        
        // Reset and try a different size
        alloc.clear();
        REQUIRE(alloc.resize(75));
        CHECK(alloc.size() == 75);
    }

    SECTION("EnsureSize behavior") {
        ScopedAlloc<float> alloc(50);
        
        // Should not resize when requested size is smaller
        CHECK_FALSE(alloc.ensureSize(25));
        CHECK(alloc.size() == 50);
        
        // Should resize when requested size is larger
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
        CHECK(buff2.get() == alloc.get()); // Should start from beginning again
    }

    SECTION("Placement bounds checking") {
        ScopedAlloc<float> alloc(100);
        
        alloc.place(60);
        CHECK(alloc.hasSizeFor(40));
        CHECK_FALSE(alloc.hasSizeFor(41));
    }
}

TEST_CASE("ScopedAlloc move semantics", "[ScopedAlloc]") {
    SECTION("Move ownership") {
        ScopedAlloc<float> source(100);
        float* originalPtr = source.get();
        
        ScopedAlloc<float> dest;
        dest.takeOwnershipFrom(std::move(source));
        
        // Check source is emptied
        CHECK(source.size() == 0);
        CHECK(source.get() == nullptr);
        
        // Check destination has taken ownership
        CHECK(dest.size() == 100);
        CHECK(dest.get() == originalPtr);
    }

    SECTION("Self-assignment in move") {
        ScopedAlloc<float> alloc(100);
        float* originalPtr = alloc.get();
        int originalSize = alloc.size();
        
        alloc.takeOwnershipFrom(std::move(alloc));
        
        // Should remain unchanged
        CHECK(alloc.size() == originalSize);
        CHECK(alloc.get() == originalPtr);
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