#pragma once
#include <ostream>
#include <ipp.h>
#include "Buffer.h"
#include "JuceHeader.h"

#pragma warning(disable:4522)

#define declareForAllTypes(T)    \
    T(8u)    \
    T(16u)   \
    T(32u)   \
    T(8s)    \
    T(16s)   \
    T(32s)   \
    T(64s)   \
    T(64f)   \
    T(32fc)  \
    T(64fc)  \
    T(8sc)   \
    T(16sc)  \
    T(32sc)  \
    T(64sc)

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
        if (this->ptr == copy.ptr)
            return *this;

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
        if (this->ptr == copy.ptr)
            return *this;

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
        if (this->sz == 0)
            return;

        if (this->ptr != nullptr) {
            jassert(alive);

            ippsFree(this->ptr);
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

        if (size <= this->sz)
            return false;

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
