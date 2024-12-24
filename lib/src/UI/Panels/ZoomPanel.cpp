#include "Panel.h"
#include "ZoomPanel.h"

#include <Definitions.h>

#include "../IConsole.h"
#include "../../App/SingletonRepo.h"
#include "../../Inter/Interactor.h"


float ZoomPanel::zoomRatio = 1.259921f; // 2^0.25

ZoomPanel::ZoomPanel(SingletonRepo* repo, ZoomContext context) :
        SingletonAccessor(repo, "ZoomPanel")
    ,	context			(context)
    ,	horz			(false)
    ,	vert			(true)
    ,	tendZoomToBottom(true)
    ,	tendZoomToTop	(true)
    ,	tendZoomToLeft	(true)
    ,	tendZoomToRight	(true)
    ,	tendZoomToCentre(true) {

    if(context.haveHorz) {
        horz.setRangeLimits(rect.xMinimum, rect.xMaximum);
        horz.setCurrentRange(rect.x, rect.w);
        horz.addListener(this);
        horz.addMouseListener(this, false);
        horz.setMouseCursor(MouseCursor::LeftRightResizeCursor);
        horz.setAutoHide(false);

        addAndMakeVisible(&horz);
    }

    if (context.haveVert) {
        vert.setRangeLimits(rect.yMinimum, rect.yMaximum);
        vert.setCurrentRange(rect.y, rect.h);
        vert.addListener(this);
        vert.addMouseListener(this, false);
        vert.setMouseCursor(MouseCursor::UpDownResizeCursor);
        vert.setAutoHide(false);

        addAndMakeVisible(&vert);
    }
}

void ZoomPanel::setBounds(int x, int y, int width, int height) {
    jassert(context.panel != nullptr);

    Component::setBounds(x, y, width, height);

    int panelWidth 	= width  - (context.haveVert ? scrollbarWidth : 0);
    int panelHeight = height - (context.haveHorz ? scrollbarWidth : 0);
    context.bounded->setBounds(0, 0, panelWidth, panelHeight);

    if(context.haveHorz) {
        horz.setBounds(0, height - scrollbarWidth, panelWidth, scrollbarWidth);
    }

    if(context.haveVert) {
        vert.setBounds(width - scrollbarWidth, 0, scrollbarWidth, height);
    }
}

void ZoomPanel::mouseEnter(const MouseEvent& e) {
    auto& console = getObj(IConsole);

    if(e.originalComponent == &horz) {
        console.setMouseUsage(false, false, true, false);
        console.setKeys(String());
    } else if (e.originalComponent == &vert) {
        console.setMouseUsage(false, false, true, false);

        String cmdStr("ctrl");
      #ifdef JUCE_MAC
        cmdStr = L"\u2318";
      #endif

        console.setKeys(cmdStr);
    }

    console.write(String());
}

void ZoomPanel::scrollBarMoved(ScrollBar* bar, double newRangeStart) {
    if (bar == &horz) {
        rect.x = newRangeStart;
        rect.w = horz.getCurrentRangeSize();
    } else if (bar == &vert) {
        rect.y = newRangeStart;
        rect.h = vert.getCurrentRangeSize();
    }

    int updateSource = context.panel->getInteractor()->getUpdateSource();
    listeners.call(&ZoomListener::zoomUpdated, updateSource);
}

const Rectangle<int> ZoomPanel::getBounds() {
    return Component::getBounds();
}

int ZoomPanel::getX() 		{ return Component::getX();			}
int ZoomPanel::getY()		{ return Component::getY();			}
int ZoomPanel::getWidth()	{ return Component::getWidth();		}
int ZoomPanel::getHeight()	{ return Component::getHeight(); 	}


void ZoomPanel::panelZoomChanged(bool commandDown) {
    listeners.call(&ZoomListener::constrainZoom);
    listeners.call(&ZoomListener::doZoomExtra, commandDown);

    if(context.haveHorz) {
        horz.removeListener	(this);
        horz.setRangeLimits	(rect.xMinimum, rect.xMaximum);
        horz.setCurrentRange(rect.x, rect.w);
        horz.addListener	(this);
    }

    if (context.haveVert) {
        vert.removeListener	(this);
        vert.setRangeLimits	(rect.yMinimum, rect.yMaximum);
        vert.setCurrentRange(rect.y, rect.h);
        vert.addListener	(this);
    }
}

void ZoomPanel::updateRange(double newLimit) {
    horz.setCurrentRange(0, newLimit);

    panelZoomChanged(false);
}

void ZoomPanel::panelComponentChanged(Component* newComponent, Component* oldComponent) {
    jassert(newComponent != nullptr);

    if(oldComponent != nullptr) {
        removeChildComponent(oldComponent);
    }

    addAndMakeVisible(newComponent);
}

void ZoomPanel::zoomIn(bool cmdDown, int mouseX, int mouseY) {
    float oldZoom;

    if (cmdDown) {
        oldZoom 		= rect.h;
        float y 		= 1 - context.panel->invertScaleY(mouseY);
        float noZoomY 	= 1 - context.panel->invertScaleYNoZoom(mouseY);
        bool usualZoom	= true;

        rect.h /= zoomRatio;

        if (noZoomY < 0.125f && tendZoomToTop) {
            y = 1 - context.panel->invertScaleY(0.);
        } else if (noZoomY > 0.875f && tendZoomToBottom) {
            rect.y = (1 - 1 / zoomRatio) * oldZoom + rect.y;
            usualZoom = false;
        } else if (noZoomY > 0.4f && noZoomY < 0.6f && tendZoomToCentre) {
            y = 0.5f;
        }

        if(usualZoom) {
            rect.y = y + (rect.y - y) / zoomRatio;
        }
    } else {
        oldZoom = rect.w;
        float x = context.panel->invertScaleX(mouseX);
        float noZoomX = context.panel->invertScaleXNoZoom(mouseX);

        rect.w /= zoomRatio;

        if (noZoomX < 0.125f && tendZoomToLeft) {
            x = context.panel->invertScaleX(0.f);
            rect.x = x + (rect.x - x) / zoomRatio;
        } else if (noZoomX > 0.875f && tendZoomToRight) {
            rect.x = (1 - 1 / zoomRatio) * oldZoom + rect.x;
        } else {
            rect.x = x + (rect.x - x) / zoomRatio;
        }
    }

    panelZoomChanged(cmdDown);
}

void ZoomPanel::zoomOut(bool cmdDown, int mouseX, int mouseY) {
    float oldZoom;

    if (cmdDown) {
        oldZoom = rect.h;
        float y = 1 - context.panel->invertScaleY(mouseY);

        rect.h *= zoomRatio;
        rect.y = y + (rect.y - y) * rect.h / oldZoom;
    } else {
        oldZoom = rect.w;
        float x = context.panel->invertScaleX(mouseX);

        rect.w *= zoomRatio;
        rect.x = x + (rect.x - x) * rect.w / oldZoom;
    }

    panelZoomChanged(cmdDown);
}

void ZoomPanel::contractToRange(Buffer<float> y) {
    float minY = 1, maxY = 0;

    if (y.empty()) {
        rect.h = 1.f;
        rect.y = 0.f;

        return;
    }

    y.minmax(minY, maxY);

    float diff = jmax(0.02f, maxY - minY);
    if(diff > 0.25f) {
        diff = 1.f;
    }

    const float padRatio = 1.4f;

    rect.h = diff * padRatio;
    NumberUtils::constrain(rect.h, 0.02f, 1.f);

    float zy = 1.f - 0.5f * (minY + maxY + rect.h);
    rect.y = jmax(0.f, zy);

    if(rect.y + rect.h > 1.f) {
        rect.y = 1.f - rect.h;
    }

    panelZoomChanged(false);
}

void ZoomPanel::zoomToAttack() {
    doZoomAction(ZoomToAttack);
}

void ZoomPanel::zoomToFull() {
    doZoomAction(ZoomToFull);
}

void ZoomPanel::doZoomAction(int action) {
    if(action == ZoomToAttack) {
        rect.x = 0;
        rect.w *= 0.2f;
    } else if (action == ZoomToFull) {
        rect.x = 0;
        rect.w = 1.f;
    }

    panelZoomChanged(false);
}

/*
 * void Updater::zoomUpdated(UpdateSource source)
{
    Ref<Cross> cross = &getObject(Cross);
    Ref<PlaybackPanel> playback = &getObject(PlaybackPanel);

    ZoomRect& surf 		= surface		->getZoomRect();
    ZoomRect& crossRect = cross			->getZoomRect();
    ZoomRect& f3Rect 	= fourier3D		->getZoomRect();
    ZoomRect& f2Rect 	= fourier2D		->getZoomRect();
    ZoomRect& e2Rect	= envelopePanel	->getZoomRect();

    ZoomPanel* surfZoom = surface		->getZoomPanel();
    ZoomPanel* crsZoom	= cross			->getZoomPanel();
    ZoomPanel* f3Zoom 	= fourier3D		->getZoomPanel();
    ZoomPanel* f2Zoom 	= fourier2D		->getZoomPanel();
    ZoomPanel* e2Zoom 	= envelopePanel	->getZoomPanel();
    ZoomPanel* e3Zoom 	= e3Panel		->getZoomPanel();

    if(source == CrossUpdate)
    {
        surf.h = crossRect.w;
        surf.y = (1 - surf.h) - crossRect.x;

        crsZoom->panelZoomChanged();
        surfZoom->panelZoomChanged();

        surface->bakeTexturesNextRepaint();

        getObject(DerivativePanel).repaint();
        cross->repaint();
        surface->repaint();

    }
    else if(source == Fourier2DUpdate)
    {
        f3Rect.h = f2Rect.w;
        f3Rect.y = (1 - f3Rect.h) - f2Rect.x;

        f2Zoom->panelZoomChanged();
        f3Zoom->panelZoomChanged();

        fourier3D->bakeTexturesNextRepaint();
        fourier2D->repaint();
        fourier3D->repaint();
    }
    else if(source == SurfaceUpdate || source == Fourier3DUpdate)
    {
        ZoomRect& sourceRect = source == SurfaceUpdate ? surf : f3Rect;

        surf.x = sourceRect.x;
        surf.w = sourceRect.w;

        f3Rect.x = sourceRect.x;
        f3Rect.w = surf.w;

        e2Rect.x = sourceRect.x;
        e2Rect.w = surf.w;

        surfZoom->panelZoomChanged();
        crsZoom->panelZoomChanged();
        e2Zoom->panelZoomChanged();
        playback->repaint();

        updateTimeBackgrounds(true);
    }
    else if(source == Env2DUpdate || source == ScratchUpdate)
    {
        envelopePanel->updateBackground(false);
        e2Zoom->panelZoomChanged();
        envelopePanel->repaint();
    }
    else if(source == Env3DUpdate)
    {
        e3Zoom->panelZoomChanged();
        e3Panel->bakeTexturesNextRepaint();
        e3Panel->repaint();
    }
    else if(source == GuideUpdate)
    {
        guidePanel->updateBackground(false);
        guidePanel->repaint();
        guidePanel->getZoomPanel()->panelZoomChanged();
    }
    else if(source == WaveshapeUpdate)
    {
        waveShaperCmpt->updateBackground(false);
        waveShaperCmpt->repaint();
        waveShaperCmpt->getZoomPanel()->panelZoomChanged();
    }
    else if(source == TubeModelUpdate)
    {
        tubeModelCmpt->updateBackground(false);
        tubeModelCmpt->repaint();
        tubeModelCmpt->getZoomPanel()->panelZoomChanged();
    }
}
 */
