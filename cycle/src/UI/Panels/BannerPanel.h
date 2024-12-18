#pragma once

#include <App/SingletonAccessor.h>
#include "JuceHeader.h"
#include <Util/MicroTimer.h>

class BannerPanel:
        public Component
    , 	public SingletonAccessor
    ,	public Timer {
public:
    BannerPanel(SingletonRepo* repo);
    ~BannerPanel() override {}

    void paint(Graphics& g) override;
    void mouseEnter(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e);
    void timerCallback() override;

private:
    float secondsSinceStart{};

    Image logo;
    MicroTimer timer;

};
