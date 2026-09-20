#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphDocument.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "Graph/PresetPresentation.h"

using namespace CycleV2;
using namespace juce;

namespace {

PresetPreviewImage jpegPreview() {
    Image image(Image::RGB, 16, 9, true);
    Graphics graphics(image);
    graphics.fillAll(Colour(0xff16212b));
    graphics.setColour(Colour(0xff43c7d0));
    graphics.drawLine(0.f, 8.f, 15.f, 1.f, 2.f);

    MemoryOutputStream encoded;
    JPEGImageFormat().writeImageToStream(image, encoded);
    return { encoded.getMemoryBlock(), 16, 9, PresetPreviewView::Spectrum };
}

NodeGraph graphWithOutput() {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Output, "output", {}));
    return graph;
}

}

TEST_CASE("Preset presentation round trips outside graph state",
        "[cycle-v2][preset][presentation][serialization]") {
    PresetPresentation presentation;
    presentation.author = "Daven";
    presentation.pack = "Factory";
    presentation.description = "A bright moving sound";
    presentation.tags = { "acid", "lead" };
    presentation.rating = 4;
    presentation.preview = jpegPreview();

    const String json = GraphSerializer().toJsonString(graphWithOutput(), presentation);
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(json);

    REQUIRE(loaded.succeeded());
    REQUIRE(loaded.presentation.author == "Daven");
    REQUIRE(loaded.presentation.pack == "Factory");
    REQUIRE(loaded.presentation.tags == StringArray { "acid", "lead" });
    REQUIRE(loaded.presentation.rating == 4);
    REQUIRE(loaded.presentation.preview.has_value());
    REQUIRE(loaded.presentation.preview->jpegData == presentation.preview->jpegData);
}

TEST_CASE("Malformed optional preset preview does not invalidate its graph",
        "[cycle-v2][preset][presentation][serialization]") {
    var encoded = GraphSerializer().writeJSON(graphWithOutput());
    auto presentation = std::make_unique<DynamicObject>();
    presentation->setProperty("version", 1);
    auto preview = std::make_unique<DynamicObject>();
    preview->setProperty("mediaType", "image/jpeg");
    preview->setProperty("width", 320);
    preview->setProperty("height", 180);
    preview->setProperty("view", "spectrum");
    preview->setProperty("data", "not jpeg data");
    presentation->setProperty("preview", var(preview.release()));
    encoded.getDynamicObject()->setProperty(
            "presetPresentation",
            var(presentation.release()));

    const GraphLoadResult loaded = GraphSerializer().readJSON(encoded);

    REQUIRE(loaded.succeeded());
    REQUIRE_FALSE(loaded.presentation.preview.has_value());
    REQUIRE(loaded.presentationWarning.isNotEmpty());
}

TEST_CASE("Graph document preserves preset presentation through graph history",
        "[cycle-v2][preset][presentation][document]") {
    GraphDocument document(graphWithOutput());
    PresetPresentation presentation;
    presentation.preview = jpegPreview();
    document.setPresentation(presentation);

    NodeGraph before = document.graph();
    NodeGraph changed = before;
    changed.addNode(GraphNodeFactory().createNode(NodeKind::GlobalInput, "global", {}));
    document.recordExternalChange(std::move(before));

    REQUIRE(document.undo());
    REQUIRE(document.presentation().preview.has_value());
    REQUIRE(document.redo());
    REQUIRE(document.presentation().preview.has_value());

    const GraphLoadResult saved = GraphSerializer().loadJsonString(document.toJson());
    REQUIRE(saved.presentation.preview.has_value());
}
