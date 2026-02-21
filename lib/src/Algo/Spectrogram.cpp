#include "Spectrogram.h"
#include "PitchTracker.h"
#include "../Algo/FFT.h"
#include "../App/SingletonRepo.h"
#include "../App/Transforms.h"
#include "../Array/Buffer.h"
#include "../Array/IterableBuffer.h"
#include "../Util/Arithmetic.h"
#include <Definitions.h>

Spectrogram::Spectrogram(SingletonRepo* repo) :
        SingletonAccessor(repo, "Spectrogram") {
}

Spectrogram::~Spectrogram() {
    magMemory   .clear();
    phaseMemory .clear();
    timeMemory  .clear();
}

void Spectrogram::calculate(IterableBuffer* iterable, int flags) {

    if (flags & CalcPhases) {
        phaseMemory.ensureSize(iterable->getTotalSize() / 2);
    }

    if (flags & SaveTime) {
        timeMemory.ensureSize(iterable->getTotalSize());
    }

    magMemory.ensureSize(iterable->getTotalSize() / 2);

    iterable->reset();

    while (iterable->hasNext()) {
        Buffer<float> source = iterable->getNext();

        Transform& fft = getObj(Transforms).chooseFFT(source.size());
        fft.forward(source);

        Buffer<float> mags   = fft.getMagnitudes();
        Buffer<float> phases = fft.getPhases();

        if (flags & LogScale) {
            mags.mul(1 / float(mags.size() - 1));
            Arithmetic::applyLogMapping(mags, 50);
        }

        if (flags & SaveTime) {
            Column timeBuf(timeMemory.place(source.size()));
            timeColumns.push_back(timeBuf);
        }

        Column magColumn(magMemory.place(source.size() / 2));
        mags.copyTo(magColumn);
        magColumns.push_back(magColumn);

        if (flags & CalcPhases) {
            Column phaseCol(phaseMemory.place(source.size() / 2));
            phases.copyTo(phaseCol);
            phaseColumns.push_back(phaseCol);
        }
    }

    if (flags & CalcPhases && flags & UnwrapPhases) {
        unwrapPhaseColumns();
    }
}

void Spectrogram::unwrapPhaseColumns() {
    int numHarmonics = phaseColumns.front().size();
    ScopedAlloc<float> memory(phaseColumns.size() + 2 * numHarmonics);

    Buffer<float> unwrapped = memory.place(phaseColumns.size());
    Buffer<float> maxima    = memory.place(numHarmonics);
    Buffer<float> minima    = memory.place(numHarmonics);

    const float pi = MathConstants<float>::pi;
    const float invConst = 1 / pi * 0.5f;

    float phaseMin, phaseMax;

    for (int harmIdx = 0; harmIdx < numHarmonics; ++harmIdx) {
        for (int col = 0; col < unwrapped.size(); ++col) {
            unwrapped[col] = phaseColumns[col][harmIdx];
        }

        unwrapped.minmax(phaseMin, phaseMax);

        float diff;
        for (int col = 1; col < unwrapped.size(); ++col) {
            diff = unwrapped[col] - unwrapped[col - 1];
            diff *= invConst;

            if (diff > 0.50001f) {
                unwrapped[col] -= 2 * pi * int(diff + 0.499989999f);
            }

            if (diff < -0.50001f) {
                unwrapped[col] += 2 * pi * int(-diff + 0.499989999f);
            }
        }

        float scaling = 1 / sqrtf(jmax(0.f, float(harmIdx + 1)));
        unwrapped.mul(scaling);

        for (int col = 0; col < unwrapped.size(); ++col) {
            phaseColumns[col][harmIdx] = unwrapped[col];
        }

        unwrapped.minmax(minima[harmIdx], maxima[harmIdx]);
    }

    float maximum = maxima.max();
    float minimum = minima.min();
    float realMax = jmax(fabsf(maximum), fabsf(minimum));
}
