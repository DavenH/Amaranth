#include "Transforms.h"

Transforms::Transforms(SingletonRepo* repo) : SingletonAccessor(repo, "Transforms") {
}

Transforms::~Transforms() {
    for (int i = 0; i < numElementsInArray(ffts); ++i) {
        ffts[i] = nullptr;
    }
}

void Transforms::init() {
    for (int i = 0; i < numElementsInArray(ffts); ++i) {
        ffts[i] = std::make_unique<Transform>();
        ffts[i]->allocate(1 << (i + 3), Transform::ScaleType::NoDivByAny, true);
    }
}
