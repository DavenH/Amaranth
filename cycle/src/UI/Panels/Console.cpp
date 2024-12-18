#include <vector>
#include <App/Doc/Document.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <UI/IConsole.h>
#include <Definitions.h>

#include "Console.h"
#include "../Layout/ResizerPullout.h"
#include "../../Binary/CycleImages.h"

using std::vector;

Console::Console(SingletonRepo* repo) :
        IConsole(repo)
    ,	firstCallback	(true)
    ,	fading			(false)
    ,	opacity			(1.f)
    ,	currentPriority	(10) {
    mouseparts = PNGImageFormat::loadFrom(CycleImages::mouseparts2_png, CycleImages::mouseparts2_pngSize);
}

void Console::init() {
  #if PLUGIN_MODE
    pullout = &getObj(ResizerPullout);

    addAndMakeVisible(pullout);
  #endif
}

void Console::timerCallback() {
    if (firstCallback) {
        stopTimer();
        startTimer(40);

        firstCallback = false;
        fading = true;
        opacity = 1.f;
    } else if (fading) {
        opacity *= 0.6f;

        if (opacity < 0.1f)
            fading = false;
    } else {
        const DocumentDetails& deets = getObj(Document).getDetails();

        text 			= deets.getName();
        keys			= "by " + deets.getAuthor();
        currentPriority = 10;
        opacity 		= 1.f;
    }

    repaint();
}

void Console::paint(Graphics& g) {
    g.setFont(FontOptions(15));
    g.setColour(Colour::greyLevel(0.09f));
    g.fillAll();

    for(int i = 0; i < getWidth() / 2 + 1; ++i)
        g.drawVerticalLine(i * 2, 0, getHeight());

    int width = getWidth();

  #if PLUGIN_MODE
    width -= 28;
  #endif

    g.setColour(Colour(1.f, 0.f, 0.8f, opacity));
    g.drawFittedText(text, 5, 0, width - 120, getHeight(), Justification::centredLeft, 1);

    g.setOpacity(1.f);
    g.setColour(Colour::greyLevel(0.4));
    g.drawFittedText(keys, 0, 0, width - 40, getHeight(), Justification::centredRight, 1);

    g.setColour(Colour::greyLevel(0.27f));
    g.drawLine(width - 36.5f, 2, width - 36.5f, getHeight() - 2);

    g.setOpacity(0.3f);

    if (usage.left)
        g.drawImage(mouseparts, width - 31, -1, 11, 25, 37, 0, 11, 25, false);

    if (usage.scroll) {
        g.drawImage(mouseparts, width - 20, 0, 6, 6, 48, 0, 6, 6, false);
        g.drawImage(mouseparts, width - 20, 15, 6, 6, 48, 15, 6, 6, false);
    }

    if (usage.middle) {
        g.drawImage(mouseparts, width - 20,  7, 6, 8, 48, 7, 6, 8, false);
    }

    if(usage.right) {
        g.drawImage(mouseparts, width - 14, -1, 11, 25, 54, 0, 11, 25, false);
    }
}

void Console::write(const String& str, int priority) {
    if (currentPriority > priority) {
        currentPriority = priority;
    }

    //ignore messages of lower priority than current
    else if (currentPriority != priority) {
        return;
    }

    text 			= str;
    firstCallback 	= true;
    opacity 		= 1.f;

    stopTimer();
    startTimer(3000);
    repaint();

    if (priority <= 3) {
        if (this->getPeer()) {
            this->getPeer()->performAnyPendingRepaintsNow();
        }
    }
}

void Console::setMouseUsage(const MouseUsage& usage) {
    this->usage = usage;
}

void Console::setMouseUsage(bool left, bool scroll, bool middle, bool right) {
    this->usage = MouseUsage(left, scroll, middle, right);
}

void Console::setKeys(const String& keys) {
    this->keys = keys;
}

void Console::reset() {
    if (currentPriority > DefaultPriority) {
        return;
    }

    const DocumentDetails& deets = getObj(Document).getDetails();

    text = deets.getName();
    keys = "by " + deets.getAuthor();
    usage = MouseUsage(false, false, false, false);

    write({}, DefaultPriority);
}

void Console::resized() {
  #if PLUGIN_MODE
    Rectangle<int> bounds = getBounds().withPosition(0, -1);
    bounds.removeFromRight(2);
    pullout->setBounds(bounds.removeFromRight(24));
  #endif
}

void Console::addPullout() {
    addAndMakeVisible(pullout);
}
