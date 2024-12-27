#pragma once

#include <App/SingletonAccessor.h>
#include "JuceHeader.h"
#include <Util/MicroTimer.h>

#include "Obj/Deletable.h"

class BannerPanel:
        public Component
    , 	public SingletonAccessor
    ,   public Deletable
    ,	public Timer {
public:
    explicit BannerPanel(SingletonRepo* repo);
    ~BannerPanel() override = default;

    void paint(Graphics& g) override;
    void mouseEnter(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e) override;
    void timerCallback() override;

private:
    float secondsSinceStart{};

    Image logo;
    MicroTimer timer;

};
