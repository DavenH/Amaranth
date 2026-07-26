#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <utility>
#include <vector>

namespace CycleV2 {

template<typename Value>
class PreparedVector {
public:
    using value_type = Value;
    using iterator = typename std::vector<Value>::iterator;
    using const_iterator = typename std::vector<Value>::const_iterator;

    PreparedVector() = default;

    PreparedVector(std::initializer_list<Value> values) :
            storage(values),
            activeSize(values.size()) {
    }

    PreparedVector(const PreparedVector& other) :
            storage(other.begin(), other.end()),
            activeSize(other.size()) {
    }

    PreparedVector& operator=(const PreparedVector& other) {
        if (this != &other) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    PreparedVector(PreparedVector&&) noexcept = default;
    PreparedVector& operator=(PreparedVector&&) noexcept = default;

    PreparedVector& operator=(std::initializer_list<Value> values) {
        assign(values.begin(), values.end());
        return *this;
    }

    PreparedVector& operator=(std::vector<Value>&& values) {
        if (!prepared) {
            storage = std::move(values);
            activeSize = storage.size();
            return *this;
        }

        assign(
                std::make_move_iterator(values.begin()),
                std::make_move_iterator(values.end()));
        return *this;
    }

    void prepare(size_t capacity) {
        storage.resize(capacity);
        activeSize = 0;
        prepared = true;
    }

    void reserve(size_t capacity) {
        if (prepared) {
            jassert(capacity <= storage.size());
            return;
        }

        storage.reserve(capacity);
    }

    void resize(size_t size) {
        if (!prepared) {
            storage.resize(size);
            activeSize = size;
            return;
        }

        if (size > storage.size()) {
            jassertfalse;
            return;
        }

        activeSize = size;
    }

    void clear() {
        if (prepared) {
            activeSize = 0;
        } else {
            storage.clear();
            activeSize = 0;
        }
    }

    template<typename Iterator>
    void assign(Iterator first, Iterator last) {
        const size_t count = (size_t) std::distance(first, last);
        if (!prepared) {
            storage.assign(first, last);
            activeSize = storage.size();
            return;
        }

        if (count > storage.size()) {
            jassertfalse;
            return;
        }

        std::copy(first, last, storage.begin());
        activeSize = count;
    }

    void assign(size_t count, const Value& value) {
        if (!prepared) {
            storage.assign(count, value);
            activeSize = storage.size();
            return;
        }

        if (count > storage.size()) {
            jassertfalse;
            return;
        }

        std::fill_n(storage.begin(), count, value);
        activeSize = count;
    }

    void push_back(const Value& value) {
        append(value);
    }

    void push_back(Value&& value) {
        append(std::move(value));
    }

    iterator begin() { return storage.begin(); }
    iterator end() { return storage.begin() + (std::ptrdiff_t) activeSize; }
    const_iterator begin() const { return storage.begin(); }
    const_iterator end() const { return storage.begin() + (std::ptrdiff_t) activeSize; }

    Value& front() {
        jassert(!empty());
        return storage.front();
    }

    const Value& front() const {
        jassert(!empty());
        return storage.front();
    }

    Value& back() {
        jassert(!empty());
        return storage[activeSize - 1];
    }

    const Value& back() const {
        jassert(!empty());
        return storage[activeSize - 1];
    }

    Value& operator[](size_t index) {
        jassert(index < activeSize);
        return storage[index];
    }

    const Value& operator[](size_t index) const {
        jassert(index < activeSize);
        return storage[index];
    }

    size_t size() const { return activeSize; }
    size_t capacity() const { return prepared ? storage.size() : storage.capacity(); }
    bool empty() const { return activeSize == 0; }
    bool isPrepared() const { return prepared; }

private:
    template<typename Item>
    void append(Item&& item) {
        if (!prepared) {
            storage.push_back(std::forward<Item>(item));
            activeSize = storage.size();
            return;
        }

        if (activeSize >= storage.size()) {
            jassertfalse;
            return;
        }

        storage[activeSize++] = std::forward<Item>(item);
    }

    std::vector<Value> storage;
    size_t activeSize {};
    bool prepared {};
};

}
