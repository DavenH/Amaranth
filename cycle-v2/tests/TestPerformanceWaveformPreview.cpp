#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Runtime/GraphPresentationModel.h"
#include "UI/CanvasChromePalette.h"
#include "UI/PerformanceWaveformPreview.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Performance waveform blends into toolbar without an enclosing border",
        "[cycle-v2][ui][performance-waveform][waveform-chrome]") {
    ScopedJuceInitialiser_GUI gui;
    PerformanceWaveformPreview view;
    view.setSize(384, 50);
    const auto image = view.createComponentSnapshot(view.getLocalBounds());
    for (int x = 0; x < image.getWidth(); ++x) {
        REQUIRE(image.getPixelAt(x, 0) == CanvasChromePalette::dockSurface);
        REQUIRE(image.getPixelAt(x, image.getHeight() - 1) == CanvasChromePalette::dockSurface);
    }
    for (int y = 0; y < image.getHeight(); ++y) {
        REQUIRE(image.getPixelAt(0, y) == CanvasChromePalette::dockSurface);
        REQUIRE(image.getPixelAt(image.getWidth() - 1, y) == CanvasChromePalette::dockSurface);
    }
    ReferenceWaveformView reference;
    reference.setSize(384, 50);
    const auto referenceImage = reference.createComponentSnapshot(reference.getLocalBounds());
    REQUIRE(referenceImage.getPixelAt(0, 0) == CanvasChromePalette::border);
}

TEST_CASE("Performance waveform retains density through cursor updates and pins Blues",
        "[cycle-v2][ui][performance-waveform][waveform][cache]") {
    ScopedJuceInitialiser_GUI gui;
    struct PaletteRestore {
        WaveformColourStyles::Style previous { WaveformColourStyles::current() };
        ~PaletteRestore() { WaveformColourStyles::setCurrent(previous); }
    } restore;
    GraphPresentationModel model;
    REQUIRE(model.refresh(NodeGraph::createDemoGraph(), 1));
    Component owner;
    PerformanceWaveformPreview view;
    owner.addAndMakeVisible(view);
    owner.setBounds(0, 0, 500, 50);
    view.setBounds(owner.getLocalBounds());
    owner.addToDesktop(ComponentPeer::windowIsTemporary);
    owner.setVisible(true);
    view.setSource(model.snapshot());
    const auto waitForImage = [&] {
        const auto deadline = Time::getMillisecondCounterHiRes() + 10000.;
        while (!(bool) view.diagnostics()["ready"] && Time::getMillisecondCounterHiRes() < deadline) {
            MessageManager::getInstance()->runDispatchLoopUntil(10);
        }
        REQUIRE((bool) view.diagnostics()["ready"]);
    };
    waitForImage();
    const auto before = view.diagnostics();
    REQUIRE((int) before["captureRequests"] == 1);
    REQUIRE(before["colourStyle"].toString() == "blues");
    WaveformColourStyles::setCurrent(WaveformColourStyles::Style::Magenta);
    const auto painted = view.createComponentSnapshot(view.getLocalBounds());
    REQUIRE(painted.isValid());
    REQUIRE(view.diagnostics()["colourStyle"].toString() == "blues");
    PlaybackCursorFrame frame;
    frame.timelineCount = 2;
    frame.timeline[0] = { 0.2f, -1.f };
    frame.timeline[1] = { 0.5f, 0.4f };
    for (int tick = 0; tick < 20; ++tick) {
        view.setSource(model.snapshot());
        frame.timeline[0].time += 0.01f;
        view.setPlaybackCursors(frame);
    }
    auto after = view.diagnostics();
    REQUIRE((int) after["cursorCount"] == 2);
    REQUIRE((float) after["cursorPositions"][0] > 0.f);
    REQUIRE((float) after["cursorPositions"][1] > (float) after["cursorPositions"][0]);
    for (const char* count : { "captureRequests", "capturePublications", "sourceBuilds", "heatBuilds", "geometryBuilds" }) {
        REQUIRE((int) after[count] == (int) before[count]);
    }
    view.setPlaybackCursors({});
    REQUIRE((int) view.diagnostics()["cursorCount"] == 0);
    view.setSize(400, 50);
    waitForImage();
    after = view.diagnostics();
    REQUIRE((int) after["captureRequests"] == 1);
    REQUIRE((int) after["sourceBuilds"] == (int) before["sourceBuilds"]);
    REQUIRE((int) after["heatBuilds"] == (int) before["heatBuilds"]);

    auto changed = model.snapshot();
    changed.previewMidiNote += 12;
    view.setSource(changed);
    view.setSource(model.snapshot());
    const auto deadline = Time::getMillisecondCounterHiRes() + 10000.;
    while ((int) view.diagnostics()["capturePublications"] < 2
            && Time::getMillisecondCounterHiRes() < deadline) {
        MessageManager::getInstance()->runDispatchLoopUntil(10);
    }
    REQUIRE((int) view.diagnostics()["captureRequests"] == 3);
    REQUIRE((int) view.diagnostics()["capturePublications"] == 2);
}
