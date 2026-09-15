#include "UnisonColumnMixer.h"

#include <Array/VecOps.h>

namespace CycleDsp {

namespace {

void rotateColumn(
        Buffer<float> source,
        Buffer<float> destination,
        int phaseSamples) {
    if (phaseSamples == 0) {
        source.copyTo(destination);
        return;
    }

    source.offset(phaseSamples).copyTo(destination);
    source.withSize(phaseSamples).copyTo(
            destination.offset(source.size() - phaseSamples));
}

}

bool UnisonColumnMixer::mix(
        Buffer<float> source,
        Buffer<float> destination,
        const float* phaseCycles,
        const float* gains,
        int voiceCount,
        Buffer<float> shifted,
        Buffer<float> interpolated,
        bool interpolate) {
    if (source.empty()
            || destination.size() != source.size()
            || shifted.size() < source.size()
            || interpolated.size() < source.size()
            || phaseCycles == nullptr
            || gains == nullptr
            || voiceCount < 1) {
        return false;
    }

    destination.zero();
    for (int voiceIndex = 0; voiceIndex < voiceCount; ++voiceIndex) {
        const double scaledPhase = (double) source.size()
                * ((double) phaseCycles[voiceIndex] + 10000.0);
        const long wholePhase = (long) scaledPhase;
        const float remainder = (float) (scaledPhase - (double) wholePhase);
        const int phaseSamples = (int) (wholePhase % source.size());
        rotateColumn(source, shifted.withSize(source.size()), phaseSamples);

        if (!interpolate || remainder == 0.f) {
            destination.addProduct(shifted.withSize(source.size()), gains[voiceIndex]);
            continue;
        }

        VecOps::mul(
                shifted.withSize(source.size()),
                1.f - remainder,
                interpolated.withSize(source.size()));
        interpolated.withSize(source.size() - 1).addProduct(
                shifted.offset(1),
                remainder);
        interpolated[source.size() - 1] =
                (1.f - remainder) * shifted[source.size() - 1]
                + remainder * shifted[0];
        destination.addProduct(
                interpolated.withSize(source.size()),
                gains[voiceIndex]);
    }
    return true;
}

}
