#include <catch2/catch_test_macros.hpp>

#include "../src/UI/RenderInvalidationAccumulator.h"

using namespace CycleV2;

namespace {

class RecordingRenderTarget final : public RenderInvalidationTarget {
public:
    uint32_t availableRenderInvalidations() const override {
        return available;
    }

    void flushRenderInvalidations(uint32_t categories) override {
        ++flushes;
        flushed |= categories;
        lastFlush = categories;
    }

    uint32_t available { 0xffffffffu };
    uint32_t flushed {};
    uint32_t lastFlush {};
    int flushes {};
};

constexpr uint32_t repaint = 1u << 0;
constexpr uint32_t bake = 1u << 1;

}

TEST_CASE("Render invalidation coalesces repeated categories at one scheduling boundary",
        "[cycle-v2][render][invalidation]") {
    RecordingRenderTarget target;
    RenderInvalidationAccumulator invalidation(target);

    invalidation.request(repaint);
    invalidation.request(repaint | bake);
    invalidation.request(bake);
    invalidation.flushNowForTests();

    REQUIRE(target.flushes == 1);
    REQUIRE(target.lastFlush == (repaint | bake));
    REQUIRE(invalidation.pendingCategories() == 0);
    REQUIRE(invalidation.diagnostics().requests == 3);
    REQUIRE(invalidation.diagnostics().scheduledFlushes == 1);
    REQUIRE(invalidation.diagnostics().completedFlushes == 1);
    REQUIRE(invalidation.diagnostics().categoryDispatches == 2);
}

TEST_CASE("Render invalidation preserves separate scheduling frames",
        "[cycle-v2][render][invalidation]") {
    RecordingRenderTarget target;
    RenderInvalidationAccumulator invalidation(target);

    invalidation.request(repaint);
    invalidation.flushNowForTests();
    invalidation.request(repaint);
    invalidation.flushNowForTests();

    REQUIRE(target.flushes == 2);
    REQUIRE(invalidation.diagnostics().completedFlushes == 2);
}

TEST_CASE("Render invalidation defers unavailable texture work without spinning",
        "[cycle-v2][render][invalidation]") {
    RecordingRenderTarget target;
    target.available = repaint;
    RenderInvalidationAccumulator invalidation(target);

    invalidation.request(repaint | bake);
    invalidation.flushNowForTests();

    REQUIRE(target.flushes == 1);
    REQUIRE(target.lastFlush == repaint);
    REQUIRE(invalidation.pendingCategories() == bake);

    target.available |= bake;
    invalidation.notifyAvailabilityChanged();
    invalidation.flushNowForTests();

    REQUIRE(target.flushes == 2);
    REQUIRE(target.lastFlush == bake);
    REQUIRE(invalidation.pendingCategories() == 0);
}
