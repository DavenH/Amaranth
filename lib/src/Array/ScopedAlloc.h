#pragma once
#include <ostream>
#include "Buffer.h"
#include "JuceHeader.h"
#include "VecOps.h"

#pragma warning(disable:4522)

template<class T>
class ScopedAlloc : public Buffer<T> {
public:
    explicit ScopedAlloc(int size);

    ScopedAlloc() {
        placementPos = 0;
        alive = false;
        reserveFront = T();
        reserveBack = T();
    }

    virtual ~ScopedAlloc() { clear(); }

    ScopedAlloc& operator=(ScopedAlloc&& copy) noexcept {
        if (this->ptr == copy.ptr) {
            return *this;
        }

        clear();

        this->ptr = copy.ptr;
        this->sz = copy.sz;
        alive = this->ptr != 0 && this->sz > 0;

        copy.ptr = 0;
        copy.sz = 0;
        copy.alive = false;

        return *this;
    }

    ScopedAlloc& operator=(const ScopedAlloc& copy) {
        if (this->ptr == copy.ptr) {
            return *this;
        }

        jassertfalse;
        clear();

        return *this;
    }

    ScopedAlloc(const ScopedAlloc& copy) {
        jassertfalse;
        clear();
    }

    void resetPlacement() { placementPos = 0; }
    bool resize(int size);

    void clear() {
        if (this->sz == 0) {
            return;
        }

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

    bool ensureSize(int size, bool andCopyExisting = false) {
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

    T reserveFront, reserveBack;
};
