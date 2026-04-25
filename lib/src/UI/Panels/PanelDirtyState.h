#pragma once

#include <cstdint>

class PanelDirtyState {
public:
    enum class Flag : uint32_t {
        None            = 0,
        Layout          = 1u << 0,
        StaticVisual    = 1u << 1,
        Overlay         = 1u << 2,
        SurfaceCache    = 1u << 3,
        Resource        = 1u << 4,
        Full            = 1u << 5
    };

    void clear() {
        flags = 0;
    }

    void clear(Flag flag) {
        flags &= ~toMask(flag);
    }

    bool any() const {
        return flags != 0;
    }

    bool isDirty(Flag flag) const {
        return (flags & toMask(flag)) != 0;
    }

    uint32_t mask() const {
        return flags;
    }

    void mark(Flag flag) {
        flags |= toMask(flag);
    }

private:
    static constexpr uint32_t toMask(Flag flag) {
        return static_cast<uint32_t>(flag);
    }

    uint32_t flags = 0;
};
