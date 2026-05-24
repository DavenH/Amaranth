#include <catch2/catch_test_macros.hpp>

#include <UI/Panels/Panel.h>
#include <UI/Panels/PanelCompositor.h>
#include <UI/Panels/PanelDirtyState.h>
#include <UI/Panels/PanelHostContext.h>
#include <UI/Panels/RenderResourceCache.h>
#include <UI/Panels/Texture.h>

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

TEST_CASE("Panel host callbacks report cursor changes", "[panel][host]") {
    PanelHostCallbacks callbacks;
    bool wasCalled = false;

    callbacks.setCursorCallback([&](Panel* panel, const juce::MouseCursor& cursor) {
        ignoreUnused(cursor);
        wasCalled = true;
        REQUIRE(panel == nullptr);
    });

    REQUIRE(callbacks.hasCursorCallback());

    callbacks.setMouseCursor(nullptr, juce::MouseCursor::CrosshairCursor);

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

TEST_CASE("Panel routes cursor changes through host callbacks without a component", "[panel][host]") {
    TestPanel panel;
    PanelHostCallbacks callbacks;
    bool wasCalled = false;

    callbacks.setCursorCallback([&](Panel* callbackPanel, const juce::MouseCursor& cursor) {
        ignoreUnused(cursor);
        wasCalled = true;
        REQUIRE(callbackPanel == &panel);
    });

    panel.setHostCallbacks(callbacks);
    panel.setPanelMouseCursor(juce::MouseCursor::PointingHandCursor);

    REQUIRE(wasCalled);
}

TEST_CASE("Panel dirty state tracks independent invalidation categories", "[panel][dirty]") {
    PanelDirtyState dirtyState;

    REQUIRE_FALSE(dirtyState.any());

    dirtyState.mark(PanelDirtyState::Flag::Layout);
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);

    REQUIRE(dirtyState.any());
    REQUIRE(dirtyState.isDirty(PanelDirtyState::Flag::Layout));
    REQUIRE(dirtyState.isDirty(PanelDirtyState::Flag::StaticVisual));

    dirtyState.clear(PanelDirtyState::Flag::Layout);

    REQUIRE_FALSE(dirtyState.isDirty(PanelDirtyState::Flag::Layout));
    REQUIRE(dirtyState.isDirty(PanelDirtyState::Flag::StaticVisual));

    dirtyState.clear();

    REQUIRE_FALSE(dirtyState.any());
}

TEST_CASE("Panel compositor collects visible dirty bounds and old bounds on layout changes", "[panel][compositor]") {
    TestPanel panel;
    PanelCompositor compositor;
    PanelHostContext context;

    panel.markDirty(PanelDirtyState::Flag::Overlay);
    context.bounds = { 10.f, 20.f, 100.f, 50.f };
    context.visible = true;

    compositor.registerOrUpdatePanel(&panel, context);

    auto dirtyBounds = compositor.collectDirtyBounds();

    REQUIRE(dirtyBounds.getNumRectangles() == 1);
    REQUIRE(dirtyBounds.getRectangle(0) == juce::Rectangle<int>(10, 20, 100, 50));

    context.bounds = { 40.f, 60.f, 120.f, 80.f };
    compositor.registerOrUpdatePanel(&panel, context);

    dirtyBounds = compositor.collectDirtyBounds();

    REQUIRE(dirtyBounds.containsRectangle(juce::Rectangle<int>(10, 20, 100, 50)));
    REQUIRE(dirtyBounds.containsRectangle(juce::Rectangle<int>(40, 60, 120, 80)));
}

TEST_CASE("Render resource cache versions textures and invalidation categories", "[panel][resources]") {
    RenderResourceCache cache;
    Texture texture;

    REQUIRE_FALSE(cache.isTracked(&texture));
    REQUIRE(cache.getVersion(&texture) == 0);

    cache.markTextureUpdated(&texture);

    REQUIRE(cache.isTracked(&texture));
    REQUIRE(cache.getVersion(&texture) == 1);

    cache.markTextureUpdated(&texture);

    REQUIRE(cache.getVersion(&texture) == 2);
    REQUIRE(cache.getInvalidationVersion(RenderResourceCache::InvalidationCategory::StaticVisual) == 0);

    cache.invalidate(RenderResourceCache::InvalidationCategory::StaticVisual);
    cache.invalidate(RenderResourceCache::InvalidationCategory::SurfaceData);

    REQUIRE(cache.getInvalidationVersion(RenderResourceCache::InvalidationCategory::StaticVisual) == 1);
    REQUIRE(cache.getInvalidationVersion(RenderResourceCache::InvalidationCategory::SurfaceData) == 1);

    cache.clear();

    REQUIRE_FALSE(cache.isTracked(&texture));
    REQUIRE(cache.getVersion(&texture) == 0);
    REQUIRE(cache.getInvalidationVersion(RenderResourceCache::InvalidationCategory::StaticVisual) == 0);
}

TEST_CASE("Render resource cache clears dirty flags after category rebuilds", "[panel][resources]") {
    RenderResourceCache cache;
    PanelDirtyState dirtyState;

    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
    dirtyState.mark(PanelDirtyState::Flag::SurfaceCache);
    dirtyState.mark(PanelDirtyState::Flag::Layout);
    dirtyState.mark(PanelDirtyState::Flag::Full);

    cache.clearDirtyFlagAfterRebuild(dirtyState, RenderResourceCache::InvalidationCategory::StaticVisual);
    cache.clearDirtyFlagAfterRebuild(dirtyState, RenderResourceCache::InvalidationCategory::SurfaceData);
    cache.clearDirtyFlagAfterRebuild(dirtyState, RenderResourceCache::InvalidationCategory::Transform);

    REQUIRE_FALSE(dirtyState.isDirty(PanelDirtyState::Flag::StaticVisual));
    REQUIRE_FALSE(dirtyState.isDirty(PanelDirtyState::Flag::SurfaceCache));
    REQUIRE_FALSE(dirtyState.isDirty(PanelDirtyState::Flag::Layout));
    REQUIRE(dirtyState.isDirty(PanelDirtyState::Flag::Full));

    cache.clearDirtyFlagAfterRebuild(dirtyState, RenderResourceCache::InvalidationCategory::FullRebuild);

    REQUIRE_FALSE(dirtyState.any());
}
