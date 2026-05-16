#include "YinPitchDetector.h"

#include <Array/VecOps.h>

#include "PitchTracker.h"
#include "../Resampling.h"
#include "../../Audio/PitchedSample.h"

void YinPitchDetector::track(PitchTracker& tracker) {
    if (tracker.sample == nullptr) {
        return;
    }

    tracker.data.octaveRatioThres    = 0.5;
    const int downsampleRate = 16000;
    const int minFrequency = roundToInt(tracker.request.minFrequencyHz);
    const int maxFrequency = roundToInt(jmin(tracker.request.maxFrequencyHz, 1500.f));

    float rateRatio   = float(downsampleRate) / tracker.sample->samplerate;
    int numSamples16k = rateRatio * tracker.sample->size();
    int length        = numSamples16k - tracker.data.offsetSamples;
    int maxlag        = downsampleRate / minFrequency;
    tracker.data.step = int(maxlag * 1.25f + jmax(0, (length - downsampleRate * 5) / 20));
    int inc           = tracker.data.step / tracker.data.overlap;
    int minlag        = downsampleRate / maxFrequency;
    int lagSize       = maxlag - minlag;
    int offset        = 0;

    ScopedAlloc<float> memory(tracker.sample->size() + numSamples16k + tracker.data.step + lagSize * 2 + 32);

    Buffer<float> wavBuff   = tracker.sample->audio.left;
    Buffer<float> wavCopy   = memory.place(tracker.sample->size() + 32);
    Buffer<float> resamp16k = memory.place(numSamples16k);
    Buffer<float> diff      = memory.place(tracker.data.step);
    Buffer<float> ramp      = memory.place(lagSize);
    Buffer<float> norms     = memory.place(lagSize);

    ramp.ramp(0, 0.3f / float(lagSize));

    {
        wavBuff.copyTo(wavCopy);

        VecOps::fir(wavBuff, wavCopy, jmin(0.5f, 0.25f / rateRatio));
    }

    Resampling::linResample(wavBuff, resamp16k);

    tracker.sample->resetPeriods();

    while (offset + tracker.data.step + maxlag < length) {
        norms.zero();

        for (int lag = minlag; lag < maxlag; ++lag) {
            int remaining = numSamples16k - (offset + lag);
            if (remaining > 0) {
                Buffer<float> d = diff.withSize(jmin(remaining, tracker.data.step));

                VecOps::sub(resamp16k + offset, resamp16k + lag + offset, d);
                norms[lag - minlag] += d.normL2();
            }
        }

        norms.mul(lagSize / norms.normL1()).add(ramp);

        int troughIndex    = tracker.getTrough(norms, minlag);
        float scaledPeriod = (float) (troughIndex + minlag) / rateRatio;
        float confidence   = norms[troughIndex];

        PitchFrame frame((int) ((float) offset / rateRatio), scaledPeriod, confidence);

        tracker.sample->addFrame(frame);

        offset += inc;
    }

    tracker.fillFrequencyBins();
    tracker.confidence = PitchTracker::refineFrames(tracker.sample, tracker.getAveragePeriod());
}
