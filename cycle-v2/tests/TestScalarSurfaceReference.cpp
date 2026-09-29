#include <catch2/catch_test_macros.hpp>
#include <UI/Panels/ScalarSurfaceMaterial.h>
#include <cstdlib>

#include "Graph/GraphSerializer.h"
#include "Runtime/GraphPresentationModel.h"
#include "UI/NodePreviewRenderer.h"
#include "UI/SignalProbeDetailView.h"

using namespace CycleV2;

TEST_CASE("Icy-hot Spy previews match saved lab programs", "[surface-program]") {
    const File fixtures = File(CYCLE_V2_SOURCE_DIR).getParentDirectory()
            .getChildFile("scripts/fixtures/surface-colour-lab");
    FileInputStream input(fixtures.getChildFile("stengah-b0-spy1.f32"));
    REQUIRE(input.openedOk());
    NodePreviewResult preview;
    preview.role = PreviewModuleRole::SignalSpy;
    preview.domain = PortDomain::TimeSignal;
    preview.gridColumns = (size_t) input.readInt();
    preview.gridRows = (size_t) input.readInt();
    preview.primary.resize(preview.gridColumns * preview.gridRows);
    for (float& value : preview.primary) {
        value = input.readFloat();
    }
    const auto source = preview.primary;
    const auto previous = ScalarSurfaceMaterial::timeSurfaceStyle();
    for (const bool program14 : { false, true }) {
        ScalarSurfaceMaterial::setTimeSurfaceStyle(program14
                ? ScalarSurfaceTimeStyle::IcyHot14 : ScalarSurfaceTimeStyle::IcyHot13);
        const auto actual = NodePreviewRenderer::createRuntimeHeatmapImage(preview, false, 1.64f);
        const auto expected = ImageFileFormat::loadFrom(fixtures.getChildFile(
                program14 ? "program-14.png" : "program-13.png"));
        CHECK(actual.isValid());
        CHECK(expected.isValid());
        int error = 0;
        for (int y = 0; y < actual.getHeight(); ++y) {
            for (int x = 0; x < actual.getWidth(); ++x) {
                const auto a = actual.getPixelAt(x, y);
                const auto b = expected.getPixelAt(x, y);
                error = jmax(error, std::abs((int) a.getRed() - b.getRed()),
                        std::abs((int) a.getGreen() - b.getGreen()),
                        std::abs((int) a.getBlue() - b.getBlue()));
            }
        }
        CHECK(error <= 3);
        CHECK(preview.primary == source);
    }
    ScalarSurfaceMaterial::setTimeSurfaceStyle(previous);
}

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
