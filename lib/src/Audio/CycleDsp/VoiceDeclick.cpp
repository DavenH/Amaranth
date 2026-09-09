#include "VoiceDeclick.h"

#include <cmath>

namespace CycleDsp {

int VoiceDeclick::attackSampleCount(double sampleRate) {
    return (int) std::ceil(attackDurationSeconds * sampleRate);
}

int VoiceDeclick::releaseSampleCount(double sampleRate) {
    return (int) std::ceil(releaseDurationSeconds * sampleRate);
}

void VoiceDeclick::prepareAttack(Buffer<float> envelope) {
    if (envelope.empty()) {
        return;
    }
    if (envelope.size() == 1) {
        envelope[0] = (float) std::exp(-std::exp(2.));
        return;
    }
    envelope.ramp(2.f, -15.f / float(envelope.size() - 1)).exp().mul(-1.f).exp();
}

void VoiceDeclick::prepareRelease(Buffer<float> envelope) {
    if (envelope.empty()) {
        return;
    }
    if (envelope.size() == 1) {
        envelope[0] = 1.f - (float) std::exp(-std::exp(2.));
        return;
    }
    envelope.ramp(2.f, -15.f / float(envelope.size() - 1))
            .exp()
            .mul(-1.f)
            .exp()
            .subCRev(1.f);
}

void VoiceDeclick::applyReleaseTail(
        Buffer<float> envelope,
        Buffer<float> releaseEnvelope,
        int releaseSamplesRemaining) {
    if (releaseSamplesRemaining <= 0) {
        return;
    }

    const int fadeSize = releaseEnvelope.size();
    const int fadeStart = jmax(0, releaseSamplesRemaining - fadeSize);
    const int releaseSamples = jmin(envelope.size(), releaseSamplesRemaining);
    const int fadeSamples = releaseSamples - fadeStart;
    if (fadeSamples <= 0) {
        return;
    }

    const int fadeEnvelopeStart = fadeSize - releaseSamplesRemaining + fadeStart;
    envelope.section(fadeStart, fadeSamples).mul(
            releaseEnvelope.section(fadeEnvelopeStart, fadeSamples));
}

}
