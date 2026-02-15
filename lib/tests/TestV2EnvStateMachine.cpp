#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/V2/State/V2EnvStateMachine.h"

TEST_CASE("V2EnvStateMachine note on resets state", "[curve][v2][env]") {
    V2EnvStateMachine machine;

    machine.forceMode(V2EnvMode::Looping);
    machine.noteOn();

    REQUIRE(machine.getMode() == V2EnvMode::Normal);
    REQUIRE_FALSE(machine.isReleasePending());
}

TEST_CASE("V2EnvStateMachine note off enters release once", "[curve][v2][env]") {
    V2EnvStateMachine machine;

    REQUIRE(machine.noteOff(true));
    REQUIRE(machine.getMode() == V2EnvMode::Releasing);
    REQUIRE(machine.isReleasePending());

    REQUIRE(machine.consumeReleaseTrigger());
    REQUIRE_FALSE(machine.consumeReleaseTrigger());
    REQUIRE_FALSE(machine.isReleasePending());

    REQUIRE_FALSE(machine.noteOff(true));
}

TEST_CASE("V2EnvStateMachine handles no-release envelopes", "[curve][v2][env]") {
    V2EnvStateMachine machine;

    REQUIRE_FALSE(machine.noteOff(false));
    REQUIRE(machine.getMode() == V2EnvMode::Normal);
    REQUIRE_FALSE(machine.isReleasePending());
}

TEST_CASE("V2EnvStateMachine enters looping only from normal", "[curve][v2][env]") {
    V2EnvStateMachine machine;

    REQUIRE(machine.transitionToLooping(true, true));
    REQUIRE(machine.getMode() == V2EnvMode::Looping);

    REQUIRE_FALSE(machine.transitionToLooping(true, true));
    machine.forceMode(V2EnvMode::Releasing);
    REQUIRE_FALSE(machine.transitionToLooping(true, true));
}

