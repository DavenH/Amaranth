#include <App/AppConstants.h>
#include "BannerPanel.h"

#include "../CycleGraphicsUtils.h"
#include "Console.h"

BannerPanel::BannerPanel(SingletonRepo* repo) : SingletonAccessor(repo, "BannerPanel") {
}

void BannerPanel::paint(Graphics& g) {
    getObj(CycleGraphicsUtils).fillBlackground(this, g);

#ifdef BEAT_EDITION
    Rectangle<int> rect = getLocalBounds();
    rect.reduce(6, 6);

    Rectangle<int> beat = rect.removeFromBottom(50);

    g.drawImageWithin(getObj(MiscGraphics).beatLogo, beat.getX(), beat.getY(), beat.getWidth(), beat.getHeight(),
                      RectanglePlacement::centred, false);

    Rectangle<int> square = rect.reduced((rect.getWidth() - rect.getHeight()) / 2, 0);
    square.expand(10, 10);
    square.translate(0, 10);

    g.drawImageWithin(getObj(MiscGraphics).logo, square.getX(), square.getY(), square.getWidth(), square.getHeight(),
                      RectanglePlacement::centred, false);


#else
    Rectangle<int> bounds = getLocalBounds();
    bounds.reduce(4, 4);

    g.setImageResamplingQuality(Graphics::lowResamplingQuality);
    g.drawImageWithin(logo, bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                      RectanglePlacement::centred | RectanglePlacement::doNotResize, false);
#endif
}


void BannerPanel::mouseEnter(const MouseEvent& e) {
#ifdef BEAT_EDITION
    String line("CycleBE v1.0.");
    line << String(getConstant(BuildNumber));
#else
    String line("Cycle v" + String(getRealConstant(ProductVersion), 1) + " build " + String(getConstant(BuildNumber)));
#endif

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
