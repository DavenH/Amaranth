#include "Graph/SharedAudioSamples.h"

#include <utility>

#include "Graph/InteractionComplexityDiagnostics.h"

namespace CycleV2 {

SharedAudioSamples::SharedAudioSamples() : values(std::make_shared<Container>()) {}

SharedAudioSamples::SharedAudioSamples(std::initializer_list<float> initialValues) :
        values(std::make_shared<Container>(initialValues)) {}

SharedAudioSamples::SharedAudioSamples(Container initialValues) :
        values(std::make_shared<Container>(std::move(initialValues))) {}

SharedAudioSamples& SharedAudioSamples::operator=(Container nextValues) {
    values = std::make_shared<Container>(std::move(nextValues));
    return *this;
}

float* SharedAudioSamples::data() {
    return writable().data();
}

float& SharedAudioSamples::front() {
    return writable().front();
}

float& SharedAudioSamples::operator[](size_t index) {
    return writable()[index];
}

SharedAudioSamples::iterator SharedAudioSamples::begin() {
    return writable().begin();
}

SharedAudioSamples::iterator SharedAudioSamples::end() {
    return writable().end();
}

void SharedAudioSamples::reserve(size_t count) {
    writable().reserve(count);
}

void SharedAudioSamples::resize(size_t count) {
    writable().resize(count);
}

void SharedAudioSamples::clear() {
    writable().clear();
}

void SharedAudioSamples::push_back(float value) {
    writable().push_back(value);
}

bool SharedAudioSamples::operator==(const SharedAudioSamples& other) const {
    return values == other.values || *values == *other.values;
}

SharedAudioSamples::Container& SharedAudioSamples::writable() {
    if (!values.unique()) {
        InteractionComplexityDiagnostics::recordAudioSampleCopy(values->size());
        values = std::make_shared<Container>(*values);
    }
    return *values;
}

}
