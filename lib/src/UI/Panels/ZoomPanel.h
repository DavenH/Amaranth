#pragma once

#include "ZoomRect.h"
#include "../Layout/Bounded.h"
#include "JuceHeader.h"
#include "../../App/SingletonAccessor.h"

class Panel;

class ZoomContext {
public:
    ZoomContext(Panel* panel, Bounded* bounded, bool haveHorz = true, bool haveVert = false) :
            panel(panel)
        , 	bounded(bounded)
        ,	haveHorz(haveHorz)
        ,	haveVert(haveVert) {}

    bool haveHorz, haveVert;
    Panel* panel;
    Bounded* bounded;
};

class ZoomPanel :
        public Component
    ,	public Bounded
    ,	public SingletonAccessor
    ,	public ScrollBar::Listener {
public:
    enum { scrollbarWidth = 8 };
    enum { ZoomToAttack, ZoomToFull };

    static float zoomRatio;

    bool tendZoomToBottom;
    bool tendZoomToCentre;
    bool tendZoomToLeft;
    bool tendZoomToRight;
    bool tendZoomToTop;

    class ZoomListener {
    protected:
        std::unique_ptr<ZoomPanel> zoomPanel;

    public:
        virtual ~ZoomListener() = default;

        virtual void zoomUpdated(int updateSource) = 0;
        virtual void constrainZoom() 				{}
        virtual void doZoomExtra(bool commandDown)  {}
        ZoomPanel* getZoomPanel() 					{ return zoomPanel.get(); }
    };

    /* ----------------------------------------------------------------------------- */

    ZoomPanel(SingletonRepo* repo, ZoomContext panel);
    ~ZoomPanel() override = default;

    const Rectangle<int> getBounds() override;
    void mouseEnter(const MouseEvent& e) override;
    void panelComponentChanged(Component* newComponent, Component* oldComponent = nullptr);
    void panelZoomChanged(bool cmdDown);
    void scrollBarMoved (ScrollBar* bar, double newRangeStart) override;
    void setBounds(int x, int y, int width, int height) override;
    void updateRange(double newLimit);
    void contractToRange(Buffer<float> y);
    void zoomOut(bool cmdDown, int mouseX, int mouseY);
    void zoomIn (bool cmdDown, int mouseX, int mouseY);
    void doZoomAction(int action);
    void zoomToAttack();
    void zoomToFull();

    int getX() override;
    int getY() override;
    int getWidth() override;
    int getHeight() override;

    void setZoomContext(const ZoomContext context) { this->context = context; }
    void addListener(ZoomListener* listener) { listeners.add(listener);  }
    ZoomRect& getZoomRect()   				 { return rect; 			 }
    Component* getComponent(bool h) 		 { return h ? &horz : &vert; }

    ZoomRect rect;

private:
    bool haveHorz, haveVert;

    ZoomContext context;

    ScrollBar horz, vert;
    ListenerList<ZoomListener> listeners;
};
