#include <App/SingletonRepo.h>
#include <UI/MiscGraphics.h>
#include "SamplePlacer.h"
#include "../CycleGraphicsUtils.h"
#include <Definitions.h>

SamplePlacer::SamplePlacer(SingletonRepo* repo) :
        SingletonAccessor(repo, "SamplePlacer")
    ,	horzCut(false)
    ,	horzButton(2, 0, this, repo, "Cut Horizontally")
    ,	vertButton(2, 0, this, repo, "Cut Vertically")
    ,	pair(this) {
    addAndMakeVisible(horzButton);
    addAndMakeVisible(vertButton);
    addAndMakeVisible(pair);
}

void SamplePlacer::paint(Graphics& g) {
    getObj(CycleGraphicsUtils).fillBlackground(this, g);

    g.setColour(Colours::grey);

    horzCut ?
        g.drawHorizontalLine(xy.getY(), 0, getWidth()) :
        g.drawVerticalLine(xy.getX(), 0, getHeight());
}

void SamplePlacer::mouseDown(const MouseEvent& e) {
    cut();
}

void SamplePlacer::mouseMove(const MouseEvent& e) {
    xy = e.getPosition().toFloat() * float(1 / getWidth());

    repaint();
}

void SamplePlacer::cut() {
}

void SamplePlacer::buttonClicked(Button* button) {
    horzCut = button == &horzButton;
    repaint();
}

void SamplePlacer::resized() {
    Rectangle<int> r = getLocalBounds();
    pair.setBounds(r);
}

SamplePlacerPanel::SamplePlacerPanel(SingletonRepo* repo) : SingletonAccessor(repo, "SamplePlacerPanel")
    ,	samplePlacer(repo) {
    addAndMakeVisible(&samplePlacer);
}

void SamplePlacerPanel::resized() {
    Rectangle<int> r = getLocalBounds();
    r.reduce(40, 40);

    samplePlacer.setBounds(r.removeFromLeft(300));
}

SamplePair::SamplePair(SamplePlacer* placer) :
    placer(placer),
    horz(false) {
}

void SamplePair::split(float portion, bool horz) {
    this->portion = portion;
    this->horz = horz;

    jassert(a == nullptr);
    jassert(b == nullptr);

    a = std::make_unique<SamplePair>(placer.get());
    b = std::make_unique<SamplePair>(placer.get());
    dragger = std::make_unique<SampleDragger>(this, horz);

    addAndMakeVisible(*a);
    addAndMakeVisible(*b);
    addAndMakeVisible(*dragger);

    SamplePair* bigger = (portion > 0.5 ? b : a).get();
    bigger->file = file;
    file = File();

    resized();
}

void SamplePair::paint(Graphics& g) {
    const Rectangle<int> r = getLocalBounds();

    g.setColour(Colour::greyLevel(0.2f));
    g.fillRect(r);

    g.setColour(Colour::greyLevel(0.3f));
    g.drawRect(r);

    if (file.getFullPathName().isEmpty()) {
        g.drawImageWithin(placer->folderImage, 0, 0, getWidth(), getHeight(), RectanglePlacement::centred);
    } else {
        Font font(15);

        g.setFont(font);

        placer->getObj(MiscGraphics).drawCentredText(g, getBounds(), file.getFileName());
    }
}

void SamplePair::resized() {
    Rectangle<int> r = getLocalBounds();

    int x = r.getX();
    int y = r.getY();
    int width = r.getWidth();
    int height = r.getHeight();

    if (horz) {
        int firstWidth, secondWidth;

        firstWidth = roundToInt((width - border) * portion);
        secondWidth = roundToInt((width - border) * (1 - portion));

        if (a != nullptr) {
            a->setBounds(x, y, firstWidth, height);
        }

        if (b != nullptr) {
            b->setBounds(x + firstWidth + border, y, secondWidth, height);
        }

        if (dragger) {
            dragger->setBounds(x + firstWidth, y - 2, border + 2, height + 4);
        }
    } else {
        int firstHeight, secondHeight;

        firstHeight = roundToInt((height - border) * portion);
        secondHeight = roundToInt((height - border) * (1 - portion));

        if (a != nullptr) {
            a->setBounds(x, y, width, firstHeight);
        }

        if (b != nullptr) {
            b->setBounds(x, y + firstHeight + border, width, secondHeight);
        }

        if (dragger != nullptr) {
            dragger->setBounds(x - 2, y + firstHeight, width + 4, border);
        }
    }
}
