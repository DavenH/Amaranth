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
        ffts[i]->setFFTScaleType(IPP_FFT_NODIV_BY_ANY);
        ffts[i]->allocate(1 << (i + 3), true);
    }
}
