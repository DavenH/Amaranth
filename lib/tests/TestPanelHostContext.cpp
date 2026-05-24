#include <catch2/catch_test_macros.hpp>

#include <UI/Panels/Panel.h>
#include <UI/Panels/PanelHostContext.h>

namespace {

class TestPanel :
        public Panel {
public:
    TestPanel() :
            SingletonAccessor(nullptr, "TestPanel")
        ,   Panel(nullptr, "TestPanel")
    {}

    void drawDepthLinesAndVerts() override {}
};

}

TEST_CASE("Panel host context converts host positions into panel-local coordinates", "[panel][host]") {
    juce::Rectangle<float> bounds(20.f, 40.f, 300.f, 160.f);

    REQUIRE(PanelHostContext::containsHostPoint({ 25.f, 45.f }, bounds));
    REQUIRE_FALSE(PanelHostContext::containsHostPoint({ 19.f, 45.f }, bounds));

    auto local = PanelHostContext::hostToLocal({ 125.f, 90.f }, bounds);

    REQUIRE(local.x == 105.f);
    REQUIRE(local.y == 50.f);
}

TEST_CASE("Panel host context creates render and pointer contexts", "[panel][host]") {
    PanelHostContext context;

    context.bounds = { 10.f, 20.f, 200.f, 100.f };
    context.clip = { 25.f, 30.f, 80.f, 40.f };
    context.dirtyMask = 7;
    context.panelId = 42;
    context.scaleFactor = 2.f;
    context.usesCachedSurface = true;

    auto renderContext = context.createRenderContext();

    REQUIRE(renderContext.bounds == juce::Rectangle<int>(10, 20, 200, 100));
    REQUIRE(renderContext.clip == juce::Rectangle<int>(25, 30, 80, 40));
    REQUIRE(renderContext.dirtyMask == 7);
    REQUIRE(renderContext.panelId == 42);
    REQUIRE(renderContext.scaleFactor == 2.f);
    REQUIRE(renderContext.usesCachedSurface);

    auto pointerEvent = context.createPointerEvent(
        { 60.f, 70.f },
        juce::ModifierKeys::leftButtonModifier,
        2
    );

    REQUIRE(pointerEvent.localPosition.x == 50.f);
    REQUIRE(pointerEvent.localPosition.y == 50.f);
    REQUIRE(pointerEvent.bounds == context.bounds);
    REQUIRE(pointerEvent.clickCount == 2);
    REQUIRE(pointerEvent.leftButtonDown);
    REQUIRE_FALSE(pointerEvent.middleButtonDown);
    REQUIRE_FALSE(pointerEvent.rightButtonDown);
}

TEST_CASE("Panel host callbacks report repaint requests", "[panel][host]") {
    PanelHostCallbacks callbacks;
    bool wasCalled = false;
    auto expectedFlag = PanelDirtyState::Flag::Overlay;

    callbacks.setRepaintCallback([&](Panel* panel, PanelDirtyState::Flag flag) {
        wasCalled = true;
        REQUIRE(panel == nullptr);
        REQUIRE(flag == expectedFlag);
    });

    REQUIRE(callbacks.hasRepaintCallback());

    callbacks.requestRepaint(nullptr, expectedFlag);

    REQUIRE(wasCalled);
}

TEST_CASE("Panel uses explicit host context for bounds without a component", "[panel][host]") {
    TestPanel panel;
    PanelHostContext context;

    context.bounds = { 30.f, 50.f, 320.f, 180.f };
    context.visible = true;

    panel.setHostContext(context);

    REQUIRE(panel.isVisible());
    REQUIRE(panel.getWidth() == 320);
    REQUIRE(panel.getHeight() == 180);
    REQUIRE(panel.getBounds() == juce::Rectangle<int>(30, 50, 320, 180));
    REQUIRE(panel.getLocalBounds() == juce::Rectangle<int>(0, 0, 320, 180));
}
