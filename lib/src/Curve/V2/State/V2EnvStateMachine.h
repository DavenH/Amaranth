#pragma once

enum class V2EnvMode {
    Normal = 0,
    Looping,
    Releasing
};

class V2EnvStateMachine {
public:
    V2EnvStateMachine() = default;

    void noteOn() noexcept;
    bool noteOff(bool hasReleaseCurve) noexcept;
    bool transitionToLooping(bool canLoop, bool overextends) noexcept;
    bool consumeReleaseTrigger() noexcept;

    void forceMode(V2EnvMode mode) noexcept;
    V2EnvMode getMode() const noexcept;
    bool isReleasePending() const noexcept;

private:
    V2EnvMode mode{V2EnvMode::Normal};
    bool releasePending{false};
};

