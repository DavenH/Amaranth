#pragma once
#include "JuceHeader.h"
#include "../../Obj/Ref.h"
#include "../../App/SingletonAccessor.h"

class PanelPair;
class SingletonRepo;

class Dragger :
        public Component
    , 	public SingletonAccessor {
public:
    enum { Horz, Vert };

    enum WhatToReduceInDetail {
        ReduceNothing 		= 0,
        ReduceF3dDetail 	= 1,
        ReduceSurfDetail 	= 2,
        ReduceE3DDetail 	= 4,
    };

    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void dragStarted() = 0;
        virtual void dragEnded() = 0;
    };

    /* ----------------------------------------------------------------------------- */

    Dragger(SingletonRepo* repo, int bitfield);

    void mouseDrag(const MouseEvent& e) override;
    void mouseEnter(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void paint(Graphics& g) override;
    void reduceDetailOfPanels();
    void resized() override;
    void restoreDetailOfPanels();
    void setPanelPair(PanelPair* pair);

    void addListener(Listener* listener) 	{ listeners.add(listener); }
    PanelPair* getPair() 					{ return pair; }

    void mouseDown(const MouseEvent& e) override;
    virtual void update(int diff);

protected:
    int type, bitfield;
    int startY, startAH, startBH, startBY;
    int startX, startAW, startBW, startBX;

    Ref<PanelPair> pair;

    Image dots;
    ListenerList<Listener> listeners;

};
