#pragma once

#include "TrimeshPanel2D.h"
#include "TrimeshPanel3D.h"
#include "TrimeshInteractor2D.h"
#include "TrimeshInteractor3D.h"

#include <App/SingletonRepo.h>
#include <App/Settings.h>
#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>
#include <Inter/MorphPositioner.h>
#include <UI/IConsole.h>

#include <cstdint>
#include <functional>
#include <memory>

class GLPanelRenderer;
class CommonGL;
struct Intercept;

namespace CycleV2 {

class TrimeshPanelBridge {
public:
    TrimeshPanelBridge();
    ~TrimeshPanelBridge();

    void syncFromNode(
            const Node& node,
            int rows,
            int columns);

    TrimeshPanel3D& getPanel3D() { return panel3D; }
    TrimeshPanel2D& getPanel2D() { return panel2D; }
    TrimeshPanelDataSource& getDataSource() { return dataSource; }
    Interactor2D& getInteractor2D() { return interactor2D; }
    Interactor3D& getInteractor3D() { return interactor3D; }
    TrimeshNodeModel& getModel() { return model; }
    const std::vector<Intercept>& getRasterizerIntercepts() const { return rasterizer.getIntercepts(); }
    Component* getPanel3DHostComponent();
    Component* getPanel3DHostComponentIfCreated();
    Component* getPanel2DHostComponent();
    Component* getPanel2DHostComponentIfCreated();
    void setPanelHostCallbacks(
            std::function<void()> repaintCallback,
            std::function<void(const MouseCursor&)> cursorCallback,
            std::function<void(Point<float>)> hoverCallback);
    void initialiseSharedGlResources();
    void releaseSharedGlResources();
    void setDisplayDomain(PortDomain domain);
    void setRenderProfile(TrimeshRenderProfile profile);
    void renderPanel3D(juce::Rectangle<float> bounds, float scaleFactor);
    void renderPanel2D(juce::Rectangle<float> bounds, float scaleFactor);

private:
    class NullConsole : public IConsole {
    public:
        explicit NullConsole(SingletonRepo* repo) : IConsole(repo, "CycleV2TrimeshNullConsole") {}

        void write(const String&, int) override {}
        void setKeys(const String&) override {}
        void setMouseUsage(const MouseUsage&) override {}
        void reset() override {}
    };

    class NodeMorphPositioner : public MorphPositioner {
    public:
        void setPosition(const MorphPosition& position, int primaryAxis);

        MorphPosition getMorphPosition() override { return morph; }
        MorphPosition getOffsetPosition(bool) override { return morph; }
        int getPrimaryDimension() override { return primaryDimension; }
        float getYellow() override { return morph.time.getCurrentValue(); }
        float getRed() override { return morph.red.getCurrentValue(); }
        float getBlue() override { return morph.blue.getCurrentValue(); }
        float getValue(int dim) override;

    private:
        MorphPosition morph { 0.5f, 0.5f, 0.5f };
        int primaryDimension {};
    };

    void refreshAfterMeshEdit(bool sourceIs3D);
    void syncPrimaryAxisContext();
    void updateRasterizer(bool refresh2DPanel, bool refresh3DGeometry);
    void initialisePanel3DHost();
    void initialisePanel2DHost();
    void updatePanelHostPeers();
    void renderPanel(Panel& panel, juce::Rectangle<float> bounds, float scaleFactor);
    PanelHostCallbacks createPanelHostCallbacks();

    SingletonRepo repo;
    NullConsole console;
    NodeMorphPositioner morphPositioner;
    TrimeshNodeModel model;
    TrimeshPanelDataSource dataSource;
    Rasterization::TrilinearMeshRasterizer rasterizer;
    TrimeshInteractor2D interactor2D;
    TrimeshInteractor3D interactor3D;
    TrimeshPanel2D panel2D;
    TrimeshPanel3D panel3D;
    std::unique_ptr<Component> panel2DHost;
    std::unique_ptr<Component> panel3DHost;
    std::unique_ptr<GLPanelRenderer> panel2DRenderer;
    std::unique_ptr<GLPanelRenderer> panel3DRenderer;
    CommonGL* panel2DGfx {};
    CommonGL* panel3DGfx {};
    std::function<void()> panelHostRepaintCallback;
    std::function<void(const MouseCursor&)> panelHostCursorCallback;
    std::function<void(Point<float>)> panelHostHoverCallback;
    uint64_t lastSyncedRevision { UINT64_MAX };
    int lastRows {};
    int lastColumns {};
    bool panel3DHostInitialised {};
    bool panel2DHostInitialised {};
    bool sharedGlResourcesInitialised {};
};

}
