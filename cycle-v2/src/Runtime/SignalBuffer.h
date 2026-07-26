#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <utility>
#include <vector>

#include <Array/Buffer.h>
#include <Array/VecOps.h>

namespace CycleV2 {

class SignalBuffer {
public:
    using value_type = float;
    using iterator = float*;
    using const_iterator = const float*;

    SignalBuffer() = default;

    SignalBuffer(std::initializer_list<float> values) :
            owned(values) {
    }

    SignalBuffer(const std::vector<float>& values) :
            owned(values) {
    }

    SignalBuffer(std::vector<float>&& values) noexcept :
            owned(std::move(values)) {
    }

    SignalBuffer(const SignalBuffer& other) :
            owned(other.begin(), other.end()) {
    }

    SignalBuffer& operator=(const SignalBuffer& other) {
        if (this != &other) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    SignalBuffer(SignalBuffer&& other) noexcept {
        moveFrom(std::move(other));
    }

    SignalBuffer& operator=(SignalBuffer&& other) noexcept {
        if (this != &other) {
            owned.clear();
            capacityView = {};
            activeSize = 0;
            bound = false;
            moveFrom(std::move(other));
        }
        return *this;
    }

    SignalBuffer& operator=(const std::vector<float>& values) {
        assign(values.begin(), values.end());
        return *this;
    }

    SignalBuffer& operator=(std::vector<float>&& values) {
        if (bound) {
            assign(values.begin(), values.end());
        } else {
            owned = std::move(values);
        }
        return *this;
    }

    void bind(Buffer<float> capacity) {
        owned.clear();
        capacityView = capacity;
        activeSize = 0;
        bound = true;
    }

    void reserve(size_t requestedCapacity) {
        if (bound) {
            jassert(requestedCapacity <= (size_t) capacityView.size());
            return;
        }

        owned.reserve(requestedCapacity);
    }

    void resize(size_t requestedSize) {
        if (!bound) {
            owned.resize(requestedSize);
            return;
        }

        if (requestedSize > (size_t) capacityView.size()) {
            jassertfalse;
            activeSize = 0;
            return;
        }

        activeSize = requestedSize;
    }

    void clear() {
        if (bound) {
            activeSize = 0;
        } else {
            owned.clear();
        }
    }

    template<typename Iterator>
    void assign(Iterator first, Iterator last) {
        const size_t count = (size_t) std::distance(first, last);
        if (!bound) {
            owned.assign(first, last);
            return;
        }

        if (count > (size_t) capacityView.size()) {
            jassertfalse;
            activeSize = 0;
            return;
        }

        std::copy(first, last, capacityView.begin());
        activeSize = count;
    }

    void assign(size_t count, float value) {
        resize(count);
        if (size() == count) {
            Buffer<float>(data(), (int) count).set(value);
        }
    }

    float* data() {
        return bound ? capacityView.get() : owned.data();
    }

    const float* data() const {
        return bound ? capacityView.get() : owned.data();
    }

    iterator begin() { return data(); }
    iterator end() { return data() + size(); }
    const_iterator begin() const { return data(); }
    const_iterator end() const { return data() + size(); }

    float& front() {
        jassert(!empty());
        return data()[0];
    }

    const float& front() const {
        jassert(!empty());
        return data()[0];
    }

    float& back() {
        jassert(!empty());
        return data()[size() - 1];
    }

    const float& back() const {
        jassert(!empty());
        return data()[size() - 1];
    }

    float& operator[](size_t index) {
        jassert(index < size());
        return data()[index];
    }

    const float& operator[](size_t index) const {
        jassert(index < size());
        return data()[index];
    }

    size_t size() const {
        return bound ? activeSize : owned.size();
    }

    size_t capacity() const {
        return bound ? (size_t) capacityView.size() : owned.capacity();
    }

    bool empty() const {
        return size() == 0;
    }

    bool isBound() const {
        return bound;
    }

    operator Buffer<float>() {
        return { data(), (int) size() };
    }

    friend bool operator==(const SignalBuffer& left, const SignalBuffer& right) {
        return left.size() == right.size()
                && std::equal(left.begin(), left.end(), right.begin());
    }

    friend bool operator!=(const SignalBuffer& left, const SignalBuffer& right) {
        return !(left == right);
    }

    friend bool operator==(const SignalBuffer& left, const std::vector<float>& right) {
        return left.size() == right.size()
                && std::equal(left.begin(), left.end(), right.begin());
    }

    friend bool operator==(const std::vector<float>& left, const SignalBuffer& right) {
        return right == left;
    }

    friend bool operator!=(const SignalBuffer& left, const std::vector<float>& right) {
        return !(left == right);
    }

    friend bool operator!=(const std::vector<float>& left, const SignalBuffer& right) {
        return !(left == right);
    }

private:
    void moveFrom(SignalBuffer&& other) {
        if (other.bound) {
            capacityView = other.capacityView;
            activeSize = other.activeSize;
            bound = true;
            other.capacityView = {};
            other.activeSize = 0;
            other.bound = false;
            return;
        }

        owned = std::move(other.owned);
    }

    std::vector<float> owned;
    Buffer<float> capacityView;
    size_t activeSize {};
    bool bound {};
};

}
