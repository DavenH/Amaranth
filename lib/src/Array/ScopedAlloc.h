#pragma once

#include <ostream>
#include <utility>

#include "Buffer.h"
#include "JuceHeader.h"
#include "VecOps.h"

#pragma warning(disable:4522)

template<class T>
class ScopedAlloc : public Buffer<T> {
public:
    explicit ScopedAlloc(int size);

    ScopedAlloc() = default;

    ScopedAlloc(const ScopedAlloc&) = delete;
    ScopedAlloc& operator=(const ScopedAlloc&) = delete;

    ScopedAlloc(ScopedAlloc&& other) noexcept :
            Buffer<T>(other.ptr, other.sz)
        ,   alive(other.alive)
        ,   placementPos(other.placementPos) {
        other.ptr = nullptr;
        other.sz = 0;
        other.alive = false;
        other.placementPos = 0;
    }

    ScopedAlloc& operator=(ScopedAlloc&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        clear();

        this->ptr = other.ptr;
        this->sz = other.sz;
        alive = other.alive;
        placementPos = other.placementPos;

        other.ptr = nullptr;
        other.sz = 0;
        other.alive = false;
        other.placementPos = 0;

        return *this;
    }

    ~ScopedAlloc() override {
        clear();
    }

    ScopedAlloc& takeOwnershipFrom(ScopedAlloc&& other) noexcept {
        return operator=(std::move(other));
    }

    void resetPlacement() { placementPos = 0; }

    // Invalidates Buffer views into the current allocation when the size changes.
    bool resize(int size) override;

    void clear() {
        if (this->ptr != nullptr) {
            jassert(alive);
            VecOps::deallocate(this->ptr);
            this->ptr = nullptr;
        }

        this->sz = 0;
        alive = false;
        placementPos = 0;
    }

    Buffer<T> place(int size) {
        jassert(placementPos + size <= Buffer<T>::sz);

        Buffer<T> buff(this->ptr + placementPos, size);
        placementPos += size;
        return buff;
    }

    bool ensureSize(int size, bool = false) {
        jassert(size >= 0);
        placementPos = 0;

        if (size <= this->sz) {
            return false;
        }

        return resize(size);
    }

    bool hasSizeFor(int size) {
        return this->sz >= placementPos + size;
    }

private:
    bool alive{};
    int placementPos{};
};
