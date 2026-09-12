#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <vector>

namespace CycleV2 {

class SharedAudioSamples {
public:
    using Container = std::vector<float>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    SharedAudioSamples();
    SharedAudioSamples(std::initializer_list<float> values);
    SharedAudioSamples(Container values);

    SharedAudioSamples& operator=(Container values);

    size_t size() const { return values->size(); }
    bool empty() const { return values->empty(); }
    size_t capacity() const { return values->capacity(); }
    const float* data() const { return values->data(); }
    const float& front() const { return values->front(); }
    const float& operator[](size_t index) const { return (*values)[index]; }
    const_iterator begin() const { return values->begin(); }
    const_iterator end() const { return values->end(); }

    float* data();
    float& front();
    float& operator[](size_t index);
    iterator begin();
    iterator end();
    void reserve(size_t count);
    void resize(size_t count);
    void clear();
    void push_back(float value);

    const Container& vector() const { return *values; }
    operator const Container&() const { return *values; }

    bool operator==(const SharedAudioSamples& other) const;
    bool operator!=(const SharedAudioSamples& other) const { return !(*this == other); }

private:
    Container& writable();

    std::shared_ptr<Container> values;
};

}
