#include <catch2/catch_test_macros.hpp>
#include <UI/Panels/ScalarSurfaceMaterial.h>
#include <cstdlib>

#include "Graph/GraphSerializer.h"
#include "Runtime/GraphPresentationModel.h"
#include "UI/NodePreviewRenderer.h"
#include "UI/SignalProbeDetailView.h"

using namespace CycleV2;

TEST_CASE("Render Stengah B0 Spy 1 material reference", "[.][surface-reference]") {
    const char* output = std::getenv("CYCLE_SURFACE_REFERENCE_DIR");
    REQUIRE(output != nullptr);
    const File directory(output);
    REQUIRE(directory.createDirectory().wasOk());
    const File preset = File(CYCLE_V2_SOURCE_DIR).getChildFile("content/presets/stengah.cyclegraph");
    const auto graph = GraphSerializer().fromJsonString(preset.loadFileAsString());
    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(graph, 1));
    // Match the UI's note naming and pitch-dependent capture length together.
    constexpr int note = 35;
    const auto resolution = SignalProbeDetailView::resolutionForMidiNote(note, 44100.0);
    const auto probe = presentation.captureProbePreview(graph, "probe", resolution, note);
    REQUIRE(probe.has_value());
    REQUIRE(probe->connected);
    REQUIRE(probe->domain == PortDomain::TimeSignal);
    REQUIRE(probe->values.size() == probe->gridColumns * probe->gridRows);
    NodePreviewResult preview;
    preview.role = PreviewModuleRole::SignalSpy;
    preview.domain = probe->domain;
    preview.primary = probe->values;
    preview.gridColumns = probe->gridColumns;
    preview.gridRows = probe->gridRows;
    preview.frequencyMidiNote = probe->frequencyMidiNote;
    const auto previous = ScalarSurfaceMaterial::timeSurfaceStyle();
    ScalarSurfaceMaterial::setTimeSurfaceStyle(ScalarSurfaceTimeStyle::BipolarShaded);
    const auto image = NodePreviewRenderer::createRuntimeHeatmapImage(preview, false, 1.64f);
    ScalarSurfaceMaterial::setTimeSurfaceStyle(previous);
    REQUIRE(image.isValid());
    REQUIRE(preview.primary == probe->values);
    FileOutputStream grid(directory.getChildFile("stengah-b0-spy1.f32"));
    REQUIRE(grid.openedOk());
    REQUIRE(grid.setPosition(0));
    REQUIRE(grid.truncate().wasOk());
    grid.writeInt((int) probe->gridColumns);
    grid.writeInt((int) probe->gridRows);
    bool wroteValues = true;
    for (const float value: probe->values) {
        wroteValues = grid.writeFloat(value) && wroteValues;
    }
    REQUIRE(wroteValues);
    FileOutputStream stream(directory.getChildFile("stengah-b0-spy1.png"));
    REQUIRE(stream.openedOk());
    REQUIRE(stream.setPosition(0));
    REQUIRE(stream.truncate().wasOk());
    REQUIRE(PNGImageFormat().writeImageToStream(image, stream));
    Image presentationImage(Image::RGB, 1640, 1000, true);
    Graphics graphics(presentationImage);
    graphics.setImageResamplingQuality(Graphics::mediumResamplingQuality);
    graphics.drawImage(image, presentationImage.getBounds().toFloat());
    FileOutputStream presented(directory.getChildFile("stengah-b0-spy1-presented.png"));
    REQUIRE(presented.openedOk());
    REQUIRE(presented.setPosition(0));
    REQUIRE(presented.truncate().wasOk());
    REQUIRE(PNGImageFormat().writeImageToStream(presentationImage, presented));
}
