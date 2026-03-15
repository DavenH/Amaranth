#pragma once

#include <array>

#include "V2StageInterfaces.h"

class V2ClampOrWrapPositionerStage :
        public V2PositionerStage {
public:
    explicit V2ClampOrWrapPositionerStage(bool wraps) noexcept :
            wraps(wraps)
    {}

    bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept override;

private:
    bool wraps{false};
};

class V2ApplyScalingPositionerStage :
        public V2PositionerStage {
public:
    bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept override;
};

class V2SortAndOrderPositionerStage :
        public V2PositionerStage {
public:
    bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept override;
};

class V2PointPathPositionerStage :
        public V2PositionerStage {
public:
    bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept override;
};

class V2CompositePositionerStage :
        public V2PositionerStage {
public:
    enum { maxStages = 8 };

    void clear() noexcept;
    bool addStage(V2PositionerStage* stage) noexcept;
    void reset() noexcept override;

    bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept override;

private:
    std::array<V2PositionerStage*, maxStages> stages{};
    int stageCount{0};
};

class V2LinearPositionerStage :
        public V2PositionerStage {
public:
    bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept override;
};

class V2CyclicPositionerStage :
        public V2PositionerStage {
public:
    bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept override;
};

class V2VoiceChainingPositionerStage :
        public V2PositionerStage {
public:
    void reset() noexcept override;

    bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept override;

private:
    std::vector<Intercept> previousIntercepts;
    bool primed{false};
};
