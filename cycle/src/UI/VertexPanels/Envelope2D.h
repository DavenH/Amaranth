#pragma once

#include <App/SingletonAccessor.h>
#include <App/Doc/Savable.h>
#include <Obj/Ref.h>

#include <UI/Panels/Panel2D.h>
#include "../TourGuide.h"

class Waveform3D;
class E3Rasterizer;
class EnvelopeInter3D;
class EnvelopeInter2D;
class OscControlPanel;
class PulloutComponent;
class RetractableCallout;
class EditWatcher;
class EnvelopeMesh;

template<class EnvelopeMesh> class MeshSelector;

class Envelope2D:
        public Panel2D
    ,   public Savable
    ,	public TourGuide {
friend class EnvelopeInter3D;

public:
    class ScrollListener : public MouseListener {
    public:
        Envelope2D* panel;

        explicit ScrollListener(Envelope2D* panel) : panel(panel) {}
        void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
    };

    class Controls : public Component, public SingletonAccessor {
    public:
        Controls(SingletonRepo* repo, Envelope2D* panel);

        void init() override;
        void destroy();
        void resized() override;
        void paint(Graphics& g) override;
        void paintOverChildren(Graphics& g) override;
        void setSelectorVisibility(bool isVisible, bool doRepaint);

        bool showLayerSelector;
        Ref<Envelope2D> panel;
    };

    explicit Envelope2D(SingletonRepo* repo);
    ~Envelope2D() override;

    /* */
    void init() override;
    void panelResized() override;
    void setZoomLimit(float limit) const { zoomPanel->rect.xMaximum = limit; }

    /* drawing */
    void drawCurvesAndSurfaces() override;
    void setListenersForEditWatcher(EditWatcher* watcher);

    /* callbacks */
	bool readXML(const XmlElement* element) override;
    void writeXML(XmlElement* element) const override;

    Component* getComponent(int which) override;
    Component* getControlsComponent() { return &controls; }

    float getLoopStart();

    void componentChanged() override;
    void createScales() override;
    void dimensionsChanged();
    void drawBackground(bool fill) override;
    void drawMouseHint();
    void drawRatioLines();
    void drawScales() override;
    void getLoopPoints(float& loopStart, float& sustain);
    void postCurveDraw() override;
//	void removeScratchProps(int index);
    void setSelectorVisibility(bool isVisible, bool doRepaint = true);
    void updateBackground(bool verticalOnly) override;
    void zoomAndRepaint();

    friend class Controls;

    enum { nullEnvIndex = -1 };

private:
    ScrollListener scrollListener;

    Ref<EnvelopeInter2D> e2Interactor;
    Ref<Waveform3D> surface;

    Controls controls;

    std::unique_ptr<MeshSelector<EnvelopeMesh> > meshSelector;
    std::unique_ptr<RetractableCallout> envSelectCO;
    std::unique_ptr<PulloutComponent> envSelectPO;

    std::unique_ptr<RetractableCallout> loopCO;
    std::unique_ptr<PulloutComponent> loopPO;

    friend class ScrollListener;
    friend class EnvelopeInter2D;
};
