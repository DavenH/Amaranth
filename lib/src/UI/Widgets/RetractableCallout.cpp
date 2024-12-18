#include "RetractableCallout.h"
#include "../MiscGraphics.h"

RetractableCallout::RetractableCallout(
    const vector<Component*>& _icons,
    PulloutComponent* _pullout,
    bool _horz) {
    horz = _horz;
    icons = _icons;
    pullout = _pullout;

    alwaysCollapsed = false;
}

void RetractableCallout::paint(Graphics& g) {
}

void RetractableCallout::paintOverChildren(Graphics& g) {
}

void RetractableCallout::resized() {
    int threshSize = getExpandedSize();

    if ((horz ? getWidth() : getHeight()) < threshSize || alwaysCollapsed) {
        for (auto& icon : icons) {
            removeChildComponent(icon);
        }

        if (!isParentOf(pullout)) {
            addAndMakeVisible(pullout);
        }

        pullout->setBounds(0, 0, horz ? 24 : icons.size() * 25, horz ? icons.size() * 25 : 24);

    } else {
        int count = 0;
        int cumeSize = 0;

        if (isParentOf(pullout))
            removeChildComponent(pullout);

        for (auto& icon : icons) {
            removeChildComponent(icon);
        }

        for (auto& icon : icons) {
            addAndMakeVisible(icon);

            cumeSize = count++ * 24;
            icon->setBounds(horz ? cumeSize : 0, horz ? 0 : cumeSize, 24, 24);
        }
    }
}

void RetractableCallout::moved() {
    pullout->moved();
}

int RetractableCallout::getExpandedSize() const {
    if (isAlwaysCollapsed()) {
        return getCollapsedSize();
    }

    return icons.size() * 25 + 1;
}

int RetractableCallout::getCollapsedSize() const {
    return 25;
}

bool RetractableCallout::isCollapsed() {
    return horz ? getWidth() < getExpandedSize() : getHeight() < getExpandedSize();
}

void RetractableCallout::setAlwaysCollapsed(bool shouldBe) {
    alwaysCollapsed = shouldBe;
}

void RetractableCallout::setBoundsDelegate(int x, int y, int w, int h) {
    setBounds(x, y, w, h);
}

Rectangle<int> RetractableCallout::getBoundsInParentDelegate() const {
    return getBoundsInParent();
}

int RetractableCallout::getXDelegate() {
    return getX();
}

int RetractableCallout::getYDelegate() {
    return getY();
}
