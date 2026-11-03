#include <UI/Panels/TimeSurfaceStyles.h>

#include "UI/SpyFrameRenderer.h"

#include "Graph/DefaultOutputProbeResolver.h"
#include "UI/SignalProbeDetailView.h"
#include "UI/CanvasChromePalette.h"
#include "UI/SpyPlaybackRipple.h"
#include "Runtime/PreviewTraversal.h"

namespace CycleV2 {

SpyFrameResult SpyFrameRenderer::render(
        const NodeGraph& graph,
        GraphPresentationModel& presentation,
        const PresetPresentation& metadata,
        const SpyFrameRequest& request,
        String& error) {
    SpyFrameResult result;
    const auto* probe = graph.findSignalProbe(request.probeId);
    const bool output = request.probeId == DefaultOutputProbeResolver::probeId;
    if (output ? !DefaultOutputProbeResolver().resolve(graph).has_value() : probe == nullptr) {
        error = "Missing or disconnected Spy: " + request.probeId;
        return result;
    }
    if (request.width <= 0 || request.width > 16384
            || request.height <= 0 || request.height > 16384
            || (int64_t) request.width * request.height > 64 * 1024 * 1024
            || request.midiNote.has_value() && (*request.midiNote < 0 || *request.midiNote > 127)) {
        error = "Expected positive dimensions (at most 64 megapixels) and MIDI note in [0, 127]";
        return result;
    }
    if (!presentation.refreshPreviewControllerValues(graph, presentation.snapshot().graphRevision,
                request.controllers)
            || request.midiNote.has_value() && !presentation.refreshPreviewMidiNote(
                    graph, presentation.snapshot().graphRevision, *request.midiNote)) {
        error = "Could not refresh Spy preview controls";
        return result;
    }

    SignalProbeDetailState detail;
    if (!detail.capture(graph, presentation, request.probeId, PresetPreviewView::Time,
                presentation.probeNoteOffTime(request.probeId), 0)
            || detail.domain != PortDomain::TimeSignal) {
        error = "Cyclogram export requires a connected time-signal Spy";
        return result;
    }
    result.gradient = request.gradient.value_or(TimeSurfaceStyles::fromId(metadata.timeSurfaceStyle));
    const auto profile = TrimeshRenderProfile::fromSemantic({
            detail.domain, detail.scalePolicy, RenderSemanticRole::Generic }).withTimeStyle(result.gradient);
    const Image plot = NodePreviewRenderer::createRuntimeHeatmapImage(
            detail.renderResult, profile, false, 0.f, request.height);
    if (!plot.isValid()) {
        error = "Spy capture did not produce a plot";
        return result;
    }
    result.image = Image(Image::RGB, request.width, request.height, true);
    {
        Graphics graphics(result.image);
        graphics.fillAll(CanvasChromePalette::insetBackground);
        graphics.setImageResamplingQuality(Graphics::highResamplingQuality);
        graphics.drawImage(plot, result.image.getBounds().toFloat());
    }
    const PreviewTraversal traversal(detail.renderResult.gridColumns, detail.releaseDuration, detail.noteOffTime);
    for (size_t index = 0; index < request.playback.count; ++index) {
        const auto& voice = request.playback.voices[index];
        result.playbackPositions.push_back(traversal.playbackPosition(
                voice.ageSeconds / voice.durationSeconds, voice.released,
                voice.releaseAgeSeconds / voice.durationSeconds));
    }
    SpyPlaybackRipple::apply(result.image, result.playbackPositions);
    result.midiNote = detail.midiNote;
    result.noteOffTime = detail.noteOffTime;
    result.contentRevision = detail.renderResult.contentRevision;
    error.clear();
    return result;
}

}
