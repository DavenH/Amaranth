#include "WaveshaperTransfer.h"

#include <Array/VecOps.h>

WaveshaperTransfer::WaveshaperTransfer() :
        table(tableResolution) {
    clearTable();
}

void WaveshaperTransfer::clearTable() {
    table.zero();
}

void WaveshaperTransfer::rasterizeFrom(const Rasterization::SamplerView& sampler, float padding) {
    const int halfRes = tableResolution / 2;
    const double delta = (1.0 - 2.0 * padding) / double(halfRes - 1);
    const double phase = padding + 0.5 * delta;

    Buffer<float> halfTable(table + halfRes, halfRes);
    sampler.samplePerfectly(delta, halfTable, phase);

    halfTable.add(-padding).mul(1.f / (1.f - 2.f * padding));

    VecOps::flip(halfTable, table.withSize(halfRes));
    table.withSize(halfRes).mul(-1.f);
}

void WaveshaperTransfer::applyLookup(Buffer<float> buffer) const {
    for (float& sample : buffer) {
        sample = lookup(sample);
    }
}

void WaveshaperTransfer::process(Buffer<float> buffer, float preGain, float postGain) const {
    buffer.mul(preGain * 0.5f).add(0.5f).clip(0.f, 1.f);
    applyLookup(buffer);
    buffer.mul(postGain);
}

float WaveshaperTransfer::lookup(float value) const {
    const float clipped = jlimit(0.f, 1.f, value);
    const float tablePosition = clipped * (tableResolution - 1);
    const int index = (int) tablePosition;
    const float remainder = tablePosition - (float) index;
    return (1.f - remainder) * table[index] + remainder * table[(index + 1) & (tableResolution - 1)];
}
