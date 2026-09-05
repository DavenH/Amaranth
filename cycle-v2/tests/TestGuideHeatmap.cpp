#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphSerializer.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Guide/GuideCurveSnapshotProvider.h"
#include "Nodes/Guide/GuideHeatmapAsset.h"
#include "Nodes/Guide/GuideHeatmapLoader.h"
#include "Nodes/Guide/GuideHeatmapSampler.h"

using namespace CycleV2;

namespace {

MemoryBlock encodePng(const Image& image) {
    MemoryOutputStream output;
    PNGImageFormat format;
    REQUIRE(format.writeImageToStream(image, output));
    return output.getMemoryBlock();
}

GuideHeatmapAssetPtr gradientHeatmap() {
    Image image(Image::ARGB, 4, 4, true);
    for (int y = 0; y < image.getHeight(); ++y) {
        for (int x = 0; x < image.getWidth(); ++x) {
            const uint8 value = (uint8) roundToInt(255.f * (float) y / 3.f);
            image.setPixelAt(x, y, Colour::fromRGBA(value, value, value, 255));
        }
    }
    String error;
    auto asset = GuideHeatmapAsset::decode(encodePng(image), "gradient.png", error);
    REQUIRE(asset != nullptr);
    REQUIRE(error.isEmpty());
    return asset;
}

GuideHeatmapAssetPtr solidHeatmap(Colour colour, String filename) {
    Image image(Image::ARGB, 2, 2, true);
    image.clear(image.getBounds(), colour);
    String error;
    auto asset = GuideHeatmapAsset::decode(encodePng(image), std::move(filename), error);
    REQUIRE(asset != nullptr);
    REQUIRE(error.isEmpty());
    return asset;
}

GuideCurveResource horizontalGuide(float y) {
    GuideCurveResource guide;
    guide.id = "guide1";
    guide.shortLabel = "G1";
    FlatCurveModel curve;
    REQUIRE(curve.replaceVertices({
            { 1, 0.05f, y, 1.f },
            { 2, 0.95f, y, 1.f }
    }));
    guide.model = CurveNodeModelState::copyOf(curve, 2);
    return guide;
}

}

TEST_CASE("Guide heatmaps decode luminance alpha and use bottom-up bicubic coordinates",
        "[cycle-v2][guide][heatmap]") {
    Image image(Image::ARGB, 2, 2, true);
    image.setPixelAt(0, 0, Colour::fromRGBA(255, 255, 255, 255));
    image.setPixelAt(1, 0, Colour::fromRGBA(255, 0, 0, 128));
    image.setPixelAt(0, 1, Colour::fromRGBA(0, 0, 0, 255));
    image.setPixelAt(1, 1, Colour::fromRGBA(255, 255, 255, 0));
    String error;
    const auto asset = GuideHeatmapAsset::decode(encodePng(image), "values.png", error);

    REQUIRE(asset != nullptr);
    REQUIRE(asset->id().startsWith("sha256:"));
    REQUIRE(asset->id().length() == 71);
    REQUIRE(asset->mediaType() == "image/png");
    REQUIRE(asset->intensityAt(0, 0) == Catch::Approx(1.f));
    REQUIRE(asset->intensityAt(1, 0) == Catch::Approx(0.106f).margin(0.005f));
    REQUIRE(asset->intensityAt(1, 1) == 0.f);
    REQUIRE(asset->image().getPixelAt(1, 0).getRed()
            == juce::roundToInt(255.f * asset->intensityAt(1, 0)));
    REQUIRE(GuideHeatmapSampler::sampleBicubic(*asset, 0.f, 1.f)
            == Catch::Approx(1.f));
    REQUIRE(GuideHeatmapSampler::sampleBicubic(*asset, 0.f, 0.f)
            == Catch::Approx(0.f));
}

TEST_CASE("Guide heatmaps load asynchronously from image files",
        "[cycle-v2][guide][heatmap][loader]") {
    ScopedJuceInitialiser_GUI juce;
    Image image(Image::RGB, 2, 2, false);
    image.clear(image.getBounds(), Colours::white);
    const MemoryBlock png = encodePng(image);
    const File file = File("/private/tmp")
            .getNonexistentChildFile("cycle-v2-guide-heatmap", ".png");
    {
        FileOutputStream outputFile(file);
        REQUIRE(outputFile.openedOk());
        REQUIRE(outputFile.write(png.getData(), png.getSize()));
        outputFile.flush();
    }

    GuideHeatmapLoader loader;
    GuideHeatmapAssetPtr loaded;
    String loadError;
    bool completed = false;
    loader.load(file, [&](GuideHeatmapAssetPtr asset, String error) {
        loaded = std::move(asset);
        loadError = std::move(error);
        completed = true;
    });
    for (int attempt = 0; attempt < 100 && !completed; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(10);
    }

    REQUIRE(completed);
    REQUIRE(loaded != nullptr);
    REQUIRE(loadError.isEmpty());
    REQUIRE(loaded->filename() == file.getFileName());
    REQUIRE(file.deleteFile());
}

TEST_CASE("Guide heatmap paths become bipolar provider tables",
        "[cycle-v2][guide][heatmap][dsp]") {
    const auto heatmap = gradientHeatmap();
    const GuideCurveResource topGuide = horizontalGuide(1.f);
    const GuideCurveResource bottomGuide = horizontalGuide(0.f);
    GuideCurveSnapshotProvider provider;

    REQUIRE(provider.addGuide(topGuide, heatmap.get()));
    REQUIRE(provider.addGuide(bottomGuide, heatmap.get()));
    GuideCurveProvider::NoiseContext context;
    REQUIRE(provider.getTableValue(0, 0.5f, context) == Catch::Approx(-0.5f).margin(0.001f));
    REQUIRE(provider.getTableValue(1, 0.5f, context) == Catch::Approx(0.5f).margin(0.001f));
}

TEST_CASE("Guide heatmap assets deduplicate serialize and survive graph history",
        "[cycle-v2][guide][heatmap][serialization][undo]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.createGuideCurve().succeeded());
    const auto asset = gradientHeatmap();
    const GuideCurveResource* guide = document.graph().findGuideCurve("guide1");
    REQUIRE(guide != nullptr);

    REQUIRE(commands.setGuideHeatmap("guide1", guide->revision, asset).succeeded());
    REQUIRE(document.graph().getGuideHeatmaps().size() == 1);
    REQUIRE(document.graph().findGuideCurve("guide1")->heatmapAssetId == asset->id());
    REQUIRE(commands.duplicateGuideCurve("guide1").succeeded());
    REQUIRE(document.graph().getGuideHeatmaps().size() == 1);
    REQUIRE(document.graph().findGuideCurve("guide2")->heatmapAssetId == asset->id());

    const String encoded = GraphSerializer().toJsonString(document.graph());
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(encoded);
    REQUIRE(loaded.succeeded());
    REQUIRE(loaded.graph.getGuideHeatmaps().size() == 1);
    REQUIRE(loaded.graph.findGuideHeatmap(asset->id()) != nullptr);

    guide = document.graph().findGuideCurve("guide1");
    REQUIRE(commands.clearGuideHeatmap("guide1", guide->revision).succeeded());
    REQUIRE(document.graph().getGuideHeatmaps().size() == 1);
    guide = document.graph().findGuideCurve("guide2");
    REQUIRE(commands.clearGuideHeatmap("guide2", guide->revision).succeeded());
    REQUIRE(document.graph().getGuideHeatmaps().empty());
    REQUIRE(document.undo());
    REQUIRE(document.graph().getGuideHeatmaps().size() == 1);
    REQUIRE(document.redo());
    REQUIRE(document.graph().getGuideHeatmaps().empty());
}

TEST_CASE("Guide heatmap replacement rejects stale loads and deletion cleans orphan assets",
        "[cycle-v2][guide][heatmap][graph]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.createGuideCurve().succeeded());
    const auto first = solidHeatmap(Colours::black, "black.png");
    const auto second = solidHeatmap(Colours::white, "white.png");
    const GuideCurveResource* guide = document.graph().findGuideCurve("guide1");
    REQUIRE(guide != nullptr);
    const uint64_t unloadedRevision = guide->revision;

    REQUIRE(commands.setGuideHeatmap("guide1", unloadedRevision, first).succeeded());
    REQUIRE(commands.setGuideHeatmap("guide1", unloadedRevision, second).code
            == GraphEditCode::StaleRevision);
    guide = document.graph().findGuideCurve("guide1");
    REQUIRE(guide->heatmapAssetId == first->id());
    REQUIRE(commands.setGuideHeatmap("guide1", guide->revision, second).succeeded());
    REQUIRE(document.graph().findGuideHeatmap(first->id()) == nullptr);
    REQUIRE(document.graph().findGuideHeatmap(second->id()) != nullptr);

    REQUIRE(commands.removeGuideCurve("guide1").succeeded());
    REQUIRE(document.graph().getGuideHeatmaps().empty());
}
