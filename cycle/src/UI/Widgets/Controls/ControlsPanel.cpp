#include <iostream>
#include <iterator>
#include <algorithm>
#include <ipp.h>
#include <Definitions.h>
#include <App/SingletonRepo.h>
#include <UI/Layout/IDynamicSizeComponent.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/PulloutComponent.h>
#include <UI/Widgets/RetractableCallout.h>
#include <UI/MiscGraphics.h>

#include "ControlsPanel.h"
#include "../../CycleGraphicsUtils.h"

using std::ostream;

ControlsPanel::ControlsPanel(Button::Listener* parent, SingletonRepo* repo, bool fillBackground) : SingletonAccessor(
        repo, "ControlsPanel")
    , isHorz(false), parent(parent)
    , fillBackground(fillBackground) {
}

ControlsPanel::~ControlsPanel() {
    removeAllChildren();

    for (int i = 0; i < (int) componentsToDelete.size(); ++i) {
        delete componentsToDelete[i];
        componentsToDelete.set(i, nullptr);
    }
}

void ControlsPanel::paint(Graphics& g) {
    if (fillBackground) {
        getObj(CycleGraphicsUtils).fillBlackground(this, g);
    }

    g.setOpacity(0.5f);

    for(auto&& image : images) {
        g.drawImageAt(image.image, image.x, image.y);
    }

    int span = 26;
    Colour base = Colour::greyLevel(0.11f);

    static const float zero 		= 0.0 * IPP_PI;
    static const float quarter 		= 0.5 * IPP_PI;
    static const float half			= 1.0 * IPP_PI;
    static const float thrQrtr 		= 1.5 * IPP_PI;
    static const float full 		= 2.0 * IPP_PI;
    static const float oneAndQrtr 	= 2.5 * IPP_PI;
    static const float oneAndHalf 	= 3.0 * IPP_PI;
    static const float arcRadius 	= 3.f;
    static const float separation 	= 3.f;

    if (isHorz) {
        for (auto band : bands) {
            if (band->isCurrentlyCollapsed() || ! band->isVisibleDlg()) {
                continue;
            }

            const Rectangle<int>& bandRect = band->getBoundsInParentDelegate();
            Rectangle<float> rect(bandRect.getX() - 1, (getHeight() - span) / 2, bandRect.getWidth() - 3, span);

            Path strokePath;
            strokePath.addRoundedRectangle(rect.getX(), rect.getY() + 1, rect.getWidth(), rect.getHeight() - 1, 2.f);
            strokePath.applyTransform(AffineTransform::translation(-0.5f, -0.5f));

            g.setColour(base);
            g.setColour(Colours::black);
            g.strokePath(strokePath, PathStrokeType(1.f));
        }

        for (int i = 0; i < (int) dividedBands.size(); ++i) {
            if (!dividedBands[i].first->isVisibleDlg() || !dividedBands[i].second->isVisibleDlg()) {
                continue;
             }

            const Rectangle<int>& first = dividedBands[i].first->getBoundsInParentDelegate();
            const Rectangle<int>& second = dividedBands[i].second->getBoundsInParentDelegate();

            int middleX 			= first.getX() + first.getWidth();
            int middleY 			= getHeight() / 2;
            int halfSpanExCorner 	= span / 2 - arcRadius;
            int leftX 				= first.getX();
            int firstRight 			= middleX - separation / 2 + 1;
            int secondX 			= middleX + separation / 2 + 1;
            int secondRight 		= second.getRight();
            int lowerY 				= middleY + halfSpanExCorner;
            int topY 				= first.getY();

            Path path;
            path.startNewSubPath(leftX, middleY);
            path.addArc(leftX, topY, arcRadius, arcRadius, thrQrtr, full);
            path.addArc(firstRight - arcRadius, topY, arcRadius, arcRadius, zero, quarter);
            path.addArc(firstRight, middleY - 2 * arcRadius, separation, arcRadius, thrQrtr, quarter);
            path.addArc(secondX, topY, arcRadius, arcRadius, thrQrtr, full);
            path.addArc(secondRight - arcRadius, topY, arcRadius, arcRadius, zero, quarter);
            path.addArc(secondRight - arcRadius, lowerY, arcRadius, arcRadius, quarter, half);
            path.addArc(secondX, lowerY, arcRadius, arcRadius, half, thrQrtr);
            path.addArc(firstRight, middleY + arcRadius, separation, arcRadius, oneAndQrtr, thrQrtr);
            path.addArc(firstRight - arcRadius, lowerY, arcRadius, arcRadius, quarter, half);
            path.addArc(leftX, lowerY, arcRadius, arcRadius, half, thrQrtr);
            path.closeSubPath();

            g.setColour(Colours::black);
            path.applyTransform(AffineTransform::translation(-0.5f, -0.5f));
            g.strokePath(path, PathStrokeType(1.f));
        }
    } else {
        for (auto band : bands) {
            if(band->isCurrentlyCollapsed() || ! band->isVisibleDlg())
                continue;

            const Rectangle<int>& bandRect = band->getBoundsInParentDelegate();

            Path strokePath;
            strokePath.addRoundedRectangle((getWidth() - span) / 2, bandRect.getY(), span - 1, bandRect.getHeight() - 3, 2.f);
            strokePath.applyTransform(AffineTransform::translation(0.5f, -0.5f));

            g.setColour(Colours::black);
            g.strokePath(strokePath, PathStrokeType(1.f));
        }

        for (auto && dividedBand : dividedBands) {
            if (!dividedBand.first->isVisibleDlg() || !dividedBand.second->isVisibleDlg()) {
                continue;
            }

            const Rectangle<int>& first = dividedBand.first->getBoundsInParentDelegate();
            const Rectangle<int>& second = dividedBand.second->getBoundsInParentDelegate();

            float middleX 			= getWidth() / 2;
            float middleY 			= first.getY() + first.getHeight() + 1;
            float halfSpanExCorner 	= span / 2 - arcRadius;
            float leftX 			= first.getX() - 1;					// replaces topY
            float topY 				= first.getY();						// replaces leftX
            float firstLower 		= middleY - separation / 2 + 1;		// replaces firstRgiht
            float secondY 			= middleY + separation / 2 + 1;		// replaces secondX
            float secondLower 		= second.getBottom();				// replaces secondRight
            float rightXSubCorner 	= middleX + halfSpanExCorner - 1;
            float rightX 			= second.getRight();

            Path path;

            if(bandCount[dividedBand.first] > 1) {
                path.startNewSubPath(leftX, (topY + firstLower) / 2);
                path.addArc(leftX, firstLower - arcRadius, arcRadius, arcRadius, thrQrtr, half);
                path.addArc(middleX - 2 * arcRadius, firstLower, arcRadius, separation, full, oneAndHalf);
                path.addArc(leftX, secondY, arcRadius, arcRadius, full, thrQrtr);
                path.addArc(leftX, secondLower - arcRadius, arcRadius, arcRadius, thrQrtr, half);
                path.addArc(rightXSubCorner, secondLower - arcRadius, arcRadius, arcRadius, half, quarter);
                path.addArc(rightXSubCorner, secondY, arcRadius, arcRadius, quarter, zero);
                path.addArc(middleX + arcRadius, firstLower, arcRadius, separation, half, full);
                path.addArc(rightXSubCorner, firstLower - arcRadius, arcRadius, arcRadius, half, quarter);
                path.lineTo(rightX, (topY + firstLower) / 2);
            } else if (bandCount[dividedBand.second] > 1) {
                path.startNewSubPath(rightX, (secondY + secondLower) / 2);
                path.addArc(rightXSubCorner, secondY, arcRadius, arcRadius, quarter, zero);
                path.addArc(middleX + arcRadius, firstLower, arcRadius, separation, half, full);
                path.addArc(rightXSubCorner, firstLower - arcRadius, arcRadius, arcRadius, half, quarter);
                path.addArc(rightX - arcRadius, topY, arcRadius, arcRadius, oneAndQrtr, full);
                path.addArc(leftX, topY, arcRadius, arcRadius, full, thrQrtr);
                path.addArc(leftX, firstLower - arcRadius, arcRadius, arcRadius, thrQrtr, half);
                path.addArc(middleX - 2 * arcRadius, firstLower, arcRadius, separation, full, oneAndHalf);
                path.addArc(leftX, secondY, arcRadius, arcRadius, full, thrQrtr);
                path.lineTo(leftX, (secondY + secondLower) / 2);
            } else {
                path.startNewSubPath(middleX, topY);
                path.addArc(leftX, topY, arcRadius, arcRadius, full, thrQrtr);
                path.addArc(leftX, firstLower - arcRadius, arcRadius, arcRadius, thrQrtr, half);
                path.addArc(middleX - 2 * arcRadius, firstLower, arcRadius, separation, full, oneAndHalf);
                path.addArc(leftX, secondY, arcRadius, arcRadius, full, thrQrtr);
                path.addArc(leftX, secondLower - arcRadius, arcRadius, arcRadius, thrQrtr, half);
                path.addArc(rightXSubCorner, secondLower - arcRadius, arcRadius, arcRadius, half, quarter);
                path.addArc(rightXSubCorner, secondY, arcRadius, arcRadius, quarter, zero);
                path.addArc(middleX + arcRadius, firstLower, arcRadius, separation, half, full);
                path.addArc(rightXSubCorner, firstLower - arcRadius, arcRadius, arcRadius, half, quarter);
                path.addArc(rightX - arcRadius, topY, arcRadius, arcRadius, oneAndQrtr, full);
                path.closeSubPath();
            }

            g.setColour(Colours::black);
            path.applyTransform(AffineTransform::translation(0.5f, -0.5f));
            g.strokePath(path, PathStrokeType(1.f));
        }
    }
}

void ControlsPanel::addButton(IconButton* button) {
    expandableComponents.add(button);
    addAndMakeVisible(button);
}

void ControlsPanel::addDynamicSizeComponent(IDynamicSizeComponent* component, bool manageMemory) {
    jassert(component != nullptr);

    expandableComponents.add(component);

    if(manageMemory) {
        componentsToDelete.add(component);
    }
}

void ControlsPanel::addRetractableCallout(std::unique_ptr<RetractableCallout>& callout,
                                          std::unique_ptr<PulloutComponent>& pullout,
                                          SingletonRepo* repo,
                                          int posX, int posY,
                                          Component** buttonArray,
                                          int numButtons, bool horz) {
    vector<Component*> buttons;

    std::copy(buttonArray, buttonArray + numButtons,
              inserter(buttons, buttons.begin()));

    pullout.reset(new PulloutComponent(getObj(MiscGraphics).getIcon(posX, posY), buttons, repo, ! horz));
    callout.reset(new RetractableCallout(buttons, pullout.get(), horz));

    addAndMakeVisible(callout.get());
    expandableComponents.add(callout.get());
}

#define CTRLS_DEBUG 0

void ControlsPanel::resized() {
    isHorz = getWidth() > getHeight();

    int pos 				= 0;
    int expandedSize 		= 0;
    int minorSize 			= 0;
    int maxPotentialNewPos 	= 0;
    int maxSizeRemaining 	= 0;
    int excess 				= 0;

    spacers.clear();

  #if CTRLS_DEBUG
    dout << "resizing " << getName() << "\n";
    dout << "controls size: " << getBounds() << "\n";
  #endif

    for (int i = 0; i < expandableComponents.size(); ++i) {
        IDynamicSizeComponent* dsc = expandableComponents[i];

        if(! dsc->isVisibleDlg()) {
            continue;
        }

        expandedSize 	= dsc->getExpandedSize();
        minorSize 		= dsc->getMinorSize();
        excess 			= ((isHorz ? getHeight() : getWidth()) - minorSize) / 2;

      #if CTRLS_DEBUG
        dout << "expanded sizes: ";
        for(int j = i + 1; j < expandableComponents.size(); ++j)
            dout << expandableComponents[j]->getExpandedSize() + 1 << " ";
        dout << "\n";
      #endif

        maxSizeRemaining = 0;

        for (int j = i + 1; j < expandableComponents.size(); ++j) {
            IDynamicSizeComponent* dsc2 = expandableComponents[j];

            if(! dsc2->isVisibleDlg()) {
                continue;
            }

            maxSizeRemaining += (dsc2->getExpandedSize() + 1);
        }

        maxPotentialNewPos = pos + expandedSize + maxSizeRemaining;

      #if CTRLS_DEBUG
        dout << "component " << i << ":" << "\n";
        dout << "expanded size: " << expandedSize << "\n";
        dout << "max new pos: " << maxPotentialNewPos << "\n";
      #endif

        bool shouldAddExpandedSize = maxPotentialNewPos < (isHorz ? getWidth() : getHeight())
                && !dsc->isAlwaysCollapsed();

        dsc->setCurrentlyCollapsed(!shouldAddExpandedSize);

        if (shouldAddExpandedSize) {
            if (isHorz)
                dsc->setBoundsDelegate(pos, excess, expandedSize + 1, minorSize);
            else
                dsc->setBoundsDelegate(excess, pos, minorSize, expandedSize + 1);

            pos += expandedSize;
        } else {
            int collapsedSize = dsc->getCollapsedSize();

            if(isHorz)
                dsc->setBoundsDelegate(pos, excess, collapsedSize + 1, minorSize);
            else
                dsc->setBoundsDelegate(excess, pos, minorSize, collapsedSize + 1);

            pos += collapsedSize;
        }

      #if CTRLS_DEBUG
        dout << (shouldAddExpandedSize ? "adding expanded size" : "added collapsed size") << "\n";
        dout << "bounds: " << dsc->getBoundsInParentDelegate() << "\n";
        dout << "pos: " << pos << "\n";
      #endif
    }
}

void ControlsPanel::addBand(IDynamicSizeComponent* end) {
    bands.add(end);
}

void ControlsPanel::addDividedBand(IDynamicSizeComponent* start, IDynamicSizeComponent* end) {
    bandCount[start]++;
    bandCount[end]++;

    dividedBands.add(std::pair(start, end));
}

void ControlsPanel::setComponentVisibility(int index, bool isVisible) {
    if (!isPositiveAndBelow(index, expandableComponents.size())) {
        return;
    }

    IDynamicSizeComponent* dsc = expandableComponents[index];

    dsc->setVisibleDlg(isVisible);
}

void ControlsPanel::orphan() {
    parent = nullptr;
}


/*
 *
 * components to delete screws this up... how to avoid double deletion
ControlsPanel& ControlsPanel::operator =(const ControlsPanel& copy)
{
    this->isHorz 				= copy.isHorz;
    this->fillBackground 		= copy.fillBackground;
    this->miscGraphics 			= copy.miscGraphics;
    this->images 				= copy.images;
    this->expandableComponents 	= copy.expandableComponents;
    this->componentsToDelete	= copy.componentsToDelete;
    this->spacers 				= copy.spacers;
    this->parent 				= copy.parent;
    this->bandOffsets 			= copy.bandOffsets;
    this->bands 				= copy.bands;
    this->dividedBands 			= copy.dividedBands;
    this->bandCount 			= copy.bandCount;

    return *this;
}
*/
