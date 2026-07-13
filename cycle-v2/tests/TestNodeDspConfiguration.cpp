#include <catch2/catch_test_macros.hpp>

#include "../src/Runtime/NodeDspConfiguration.h"

using namespace CycleV2;

namespace {

class TestConfiguration final : public INodeDspConfiguration {
public:
    explicit TestConfiguration(int valueToUse) : value(valueToUse) {}

    AudioModuleRole role() const override { return AudioModuleRole::Waveshaper; }

    int value {};
};

}

TEST_CASE("DSP configuration publication retains stable keys and complete revisions", "[cycle-v2][runtime][configuration]") {
    NodeConfigurationPublisher publisher;
    int buildCount = 0;

    const auto build = [&]() {
        ++buildCount;
        return std::make_shared<const TestConfiguration>(buildCount);
    };

    const auto first = publisher.publish("waveshaper:a", build);
    const auto unchanged = publisher.publish("waveshaper:a", build);
    const auto changed = publisher.publish("waveshaper:b", build);

    REQUIRE(first.revision == 1);
    REQUIRE(unchanged.revision == first.revision);
    REQUIRE(unchanged.value == first.value);
    REQUIRE(changed.revision == 2);
    REQUIRE(changed.value != first.value);
    REQUIRE(buildCount == 2);
}

TEST_CASE("Failed DSP configuration construction preserves the last valid revision", "[cycle-v2][runtime][configuration]") {
    NodeConfigurationPublisher publisher;
    const auto valid = publisher.publish("valid", []() {
        return std::make_shared<const TestConfiguration>(7);
    });
    const auto failed = publisher.publish("invalid", []() {
        return std::shared_ptr<const INodeDspConfiguration> {};
    });

    REQUIRE(failed.revision == valid.revision);
    REQUIRE(failed.key == valid.key);
    REQUIRE(failed.value == valid.value);
}

TEST_CASE("Published DSP configurations outlive publisher replacement", "[cycle-v2][runtime][configuration]") {
    NodeConfigurationPublisher publisher;
    const auto first = publisher.publish("first", []() {
        return std::make_shared<const TestConfiguration>(11);
    });
    std::weak_ptr<const INodeDspConfiguration> oldLifetime = first.value;

    publisher.publish("second", []() {
        return std::make_shared<const TestConfiguration>(12);
    });

    REQUIRE_FALSE(oldLifetime.expired());
    REQUIRE(std::static_pointer_cast<const TestConfiguration>(first.value)->value == 11);
}
