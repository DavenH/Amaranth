#pragma once
#include "JuceHeader.h"
#include "../../Obj/Ref.h"
#include "../../App/SingletonAccessor.h"
#include "Obj/Deletable.h"

class PanelPair;
class SingletonRepo;

class Dragger :
        public Component
    ,   public Deletable
    , 	public SingletonAccessor {
public:
    enum { Horz, Vert };

    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void dragStarted() = 0;
        virtual void dragEnded() = 0;
    };

    /* ----------------------------------------------------------------------------- */

    explicit Dragger(SingletonRepo* repo);
    // ~Dragger() override = default;

    void mouseDrag(const MouseEvent& e) override;
    void mouseEnter(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void paint(Graphics& g) override;
    void resized() override;
    void setPanelPair(PanelPair* pair);

    void addListener(Listener* listener) 	{ listeners.add(listener); }
    PanelPair* getPair() 					{ return pair; }

    void mouseDown(const MouseEvent& e) override;
    virtual void update(int diff);

protected:
    int type;
    int startY, startAH, startBH, startBY;
    int startX, startAW, startBW, startBX;

    Ref<PanelPair> pair;

    Image dots;
    ListenerList<Listener> listeners;

};
