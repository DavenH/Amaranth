#include <catch2/catch_test_macros.hpp>

#include "UI/SignalProbePreviewTileCache.h"

using namespace CycleV2;

namespace {

GraphPreviewResult::SignalProbePreview makePreview() {
    GraphPreviewResult::SignalProbePreview preview;
    preview.probeId = "spy-1";
    preview.values = { 0.1f, 0.4f, 0.8f, 0.3f };
    preview.gridColumns = 2;
    preview.gridRows = 2;
    preview.domain = PortDomain::MeshField;
    preview.channelLayout = ChannelLayout::Mono;
    preview.sourceRole = PreviewModuleRole::MeshSurface;
    preview.frequencySampling = TraversalGridFrequencySampling::LinearBins;
    preview.frequencyMidiNote = 48;
    preview.connected = true;
    return preview;
}

}

TEST_CASE("Spy preview tile cache reuses only complete presentation keys",
        "[cycle-v2][canvas][performance][cache]") {
    SignalProbePreviewTileCache cache;
    GraphPreviewResult::SignalProbePreview preview = makePreview();
    NodeRenderSemantic semantic {
            PortDomain::MeshField,
            RenderScalePolicy::Bipolar,
            RenderSemanticRole::Generic
    };
    const Rectangle<int> bounds { 12, 24, 180, 120 };
    const auto access = [&](float scale = 1.f) {
        return cache.access(preview, semantic, bounds, scale);
    };

    cache.beginFrame();
    const SignalProbePreviewTileCacheAccess first = access();
    REQUIRE_FALSE(first.hit);
    Graphics cachedGraphics(*first.image);
    cachedGraphics.fillAll(Colours::red);
    Image output(Image::ARGB, 220, 180, true);
    Graphics outputGraphics(output);
    cache.draw(outputGraphics, first);
    REQUIRE(output.getPixelAt(bounds.getCentreX(), bounds.getCentreY()) == Colours::red);
    REQUIRE(cache.endFrame().misses == 1);

    cache.beginFrame();
    REQUIRE(access().hit);
    REQUIRE(cache.endFrame().hits == 1);

    cache.clear();
    cache.beginFrame();
    REQUIRE_FALSE(access().hit);
    REQUIRE(cache.endFrame().misses == 1);

    cache.beginFrame();
    preview.values[1] = 0.5f;
    REQUIRE_FALSE(access().hit);
    preview.gridColumns = 4;
    REQUIRE_FALSE(access().hit);
    preview.gridRows = 1;
    REQUIRE_FALSE(access().hit);
    preview.domain = PortDomain::SpectralMagnitudeSignal;
    REQUIRE_FALSE(access().hit);
    preview.channelLayout = ChannelLayout::LinkedStereo;
    REQUIRE_FALSE(access().hit);
    preview.sourceRole = PreviewModuleRole::SignalSpy;
    REQUIRE_FALSE(access().hit);
    preview.frequencySampling = TraversalGridFrequencySampling::LogarithmicBins;
    REQUIRE_FALSE(access().hit);
    preview.frequencyMidiNote = 60;
    REQUIRE_FALSE(access().hit);
    preview.connected = false;
    REQUIRE_FALSE(access().hit);
    semantic.domain = PortDomain::TimeSignal;
    REQUIRE_FALSE(access().hit);
    semantic.scalePolicy = RenderScalePolicy::Unipolar;
    REQUIRE_FALSE(access().hit);
    semantic.role = RenderSemanticRole::TimeWaveform;
    REQUIRE_FALSE(access().hit);
    REQUIRE_FALSE(access(2.f).hit);
    REQUIRE_FALSE(cache.access(preview, semantic, bounds.translated(1, 0), 2.f).hit);
    REQUIRE(cache.endFrame().misses == 14);

    cache.beginFrame();
    REQUIRE(cache.endFrame().hits == 0);
    cache.beginFrame();
    REQUIRE_FALSE(access(2.f).hit);
    REQUIRE(cache.endFrame().misses == 1);
}
