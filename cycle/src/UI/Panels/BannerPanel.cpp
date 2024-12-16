#include <App/AppConstants.h>
#include "BannerPanel.h"

#include "../CycleGraphicsUtils.h"
#include "Console.h"

BannerPanel::BannerPanel(SingletonRepo* repo) : SingletonAccessor(repo, "BannerPanel") {
}

void BannerPanel::paint(Graphics& g) {
    getObj(CycleGraphicsUtils).fillBlackground(this, g);

    Rectangle<int> bounds = getLocalBounds();
    bounds.reduce(4, 4);

    g.setImageResamplingQuality(Graphics::lowResamplingQuality);
    g.drawImageWithin(logo, bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                      RectanglePlacement::centred | RectanglePlacement::doNotResize, false);
}

void BannerPanel::mouseEnter(const MouseEvent& e) {
    String line("Cycle v" + String(ProjectInfo::versionString));

    getObj(IConsole).reset();
    getObj(IConsole).write(line, IConsole::DefaultPriority);
    getObj(IConsole).setKeys("by Amaranth Audio");
}

void BannerPanel::mouseDown(const MouseEvent& e) {
}

void BannerPanel::timerCallback() {
    timer.stop();
    secondsSinceStart += (float) timer.getDeltaTime();
    timer.start();

    repaint();
}
