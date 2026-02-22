#include "V2EnvStateMachine.h"

void V2EnvStateMachine::noteOn() noexcept {
    mode = V2EnvMode::Normal;
    releasePending = false;
}

bool V2EnvStateMachine::noteOff(bool hasReleaseCurve) noexcept {
    if (mode == V2EnvMode::Releasing || ! hasReleaseCurve) {
        return false;
    }

    mode = V2EnvMode::Releasing;
    releasePending = true;
    return true;
}

bool V2EnvStateMachine::transitionToLooping(bool canLoop, bool overextends) noexcept {
    if (mode != V2EnvMode::Normal || ! canLoop || ! overextends) {
        return false;
    }

    mode = V2EnvMode::Looping;
    return true;
}

bool V2EnvStateMachine::consumeReleaseTrigger() noexcept {
    bool wasPending = releasePending;
    releasePending = false;
    return wasPending;
}

void V2EnvStateMachine::forceMode(V2EnvMode mode) noexcept {
    this->mode = mode;
}

V2EnvMode V2EnvStateMachine::getMode() const noexcept {
    return mode;
}

bool V2EnvStateMachine::isReleasePending() const noexcept {
    return releasePending;
}

