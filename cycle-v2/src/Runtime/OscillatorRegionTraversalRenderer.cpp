#include "Runtime/OscillatorRegionTraversalRenderer.h"
#include "Runtime/PreviewTraversal.h"
#include "Nodes/Envelope/EnvelopeConfiguration.h"
#include "Nodes/Envelope/EnvelopePitchPreview.h"

#include <Audio/CycleDsp/UnisonColumnMixer.h>
#include <Util/Arithmetic.h>

#include <algorithm>

namespace CycleV2 {

void OscillatorRegionTraversalRenderer::prepare(
        const CompiledVoiceContext& context,
        size_t maximumRowCount) {
    voiceDurationSeconds = context.voiceDurationSeconds;
    layout = context.unison != nullptr && !context.unison->isEnabled()
            ? CycleDsp::UnisonVoiceLayout {}
            : context.lanes;
    pitchEnvelope = std::dynamic_pointer_cast<const EnvelopeConfiguration>(context.pitchEnvelope);
    cachedReleaseDuration = -1.;
    pitchEnvelopeUnitValues = context.pitchEnvelopeUnitValues;
    pitchEnvelopeUnitValuesByLane = context.pitchEnvelopeUnitValuesByLane;
    workspace.resize((int) (3 * maximumRowCount));
}

bool OscillatorRegionTraversalRenderer::render(
        SignalTraversalGrid& grid,
        int midiNote) {
    if (!grid.isValid()
            || grid.metadata.valueDomain != PortDomain::TimeSignal
            || layout.order < 1
            || layout.order > CycleDsp::maximumUnisonOrder
            || workspace.size() < (int) (3 * grid.rows)) {
        return false;
    }
    if (pitchEnvelope != nullptr && cachedReleaseDuration != grid.metadata.releaseDuration) {
        pitchEnvelopeUnitValuesByLane = EnvelopePitchPreview::renderLanes(
                pitchEnvelope, layout.order, EnvelopePitchPreview::defaultSampleCount,
                EnvelopePitchPreview::unisonPreviewSeed, grid.metadata.releaseDuration);
        cachedReleaseDuration = grid.metadata.releaseDuration;
    }
    workspace.resetPlacement();
    Buffer<float> source = workspace.place((int) grid.rows);
    Buffer<float> shifted = workspace.place((int) grid.rows);
    Buffer<float> interpolated = workspace.place((int) grid.rows);
    std::array<double, CycleDsp::maximumUnisonOrder> cumulativePhases {};
    std::array<float, CycleDsp::maximumUnisonOrder> phases {};
    std::array<float, CycleDsp::maximumUnisonOrder> gains {};
    const float level = CycleDsp::UnisonCore::voiceLevelScale(layout.order);
    for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
        gains[(size_t) laneIndex] = Arithmetic::getRelativePan(
                layout[laneIndex].pan,
                0.5f) * level;
    }

    const PreviewTraversal traversal(grid.columns, grid.metadata.releaseDuration);
    const double baseFrequency = CycleDsp::UnisonCore::frequencyForMidiNote(midiNote);
    for (size_t column = 0; column < grid.columns; ++column) {
        const double normalizedTimePerColumn = grid.columns > 1 ? traversal.increment(column) : 0.;
        const double secondsPerColumn = normalizedTimePerColumn * (double) voiceDurationSeconds;
        Buffer<float> destination(
                grid.values.data() + column * grid.rows,
                (int) grid.rows);
        destination.copyTo(source);
        source.add(-source.mean()).mul(0.5f);
        for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
            const auto& lane = layout[laneIndex];
            phases[(size_t) laneIndex] = (float) (
                    cumulativePhases[(size_t) laneIndex] + lane.phaseCycles);
            const double pitchSemitones = CycleDsp::UnisonCore::pitchSemitonesForUnitValue(
                    pitchUnitValue(laneIndex, column, grid.columns));
            const double pitchFrequencyOffset = CycleDsp::UnisonCore::frequencyForMidiPitch(
                    (double) midiNote + pitchSemitones) - baseFrequency;
            const double unisonFrequencyOffset = CycleDsp::UnisonCore::frequencyForMidiNote(
                    midiNote,
                    lane.detuneCents) - baseFrequency;
            cumulativePhases[(size_t) laneIndex] +=
                    normalizedTimePerColumn * pitchFrequencyOffset
                    + secondsPerColumn * unisonFrequencyOffset;
        }
        if (!CycleDsp::UnisonColumnMixer::mix(
                source,
                destination,
                phases.data(),
                gains.data(),
                layout.order,
                shifted,
                interpolated,
                true)) {
            return false;
        }
    }
    return true;
}

float OscillatorRegionTraversalRenderer::pitchUnitValue(
        int laneIndex,
        size_t column,
        size_t columnCount) const {
    const auto& values = laneIndex >= 0
                    && laneIndex < (int) pitchEnvelopeUnitValuesByLane.size()
            ? pitchEnvelopeUnitValuesByLane[(size_t) laneIndex]
            : pitchEnvelopeUnitValues;
    if (values.empty()) {
        return 0.5f;
    }
    if (columnCount <= 1 || values.size() == 1) {
        return values.front();
    }

    const size_t index = std::min(
            values.size() - 1,
            column * values.size() / columnCount);
    return values[index];
}

}
