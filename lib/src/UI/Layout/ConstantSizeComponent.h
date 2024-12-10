#pragma once

#include "IDynamicSizeComponent.h"
#include "JuceHeader.h"
using namespace juce;

class ConstantSizeComponent :
        public IDynamicSizeComponent,
        public Component {
private:
    bool isCurrentCollapsed;
    int size;

public:
    ConstantSizeComponent(int size)
            : size(size) {
    }

    int getExpandedSize() {
        return size;
    }

    int getCollapsedSize() {
        return getExpandedSize();
    }

    void setBoundsDelegate(int x, int y, int w, int h) {
        setBounds(x, y, w, h);
    }

    const Rectangle<int> getBoundsInParentDelegate() {
        return getBoundsInParent();
    }

    int getYDelegate() {
        return getY();
    }

    int getXDelegate() {
        return getX();
    }

    void setCurrentlyCollapsed(bool isIt) {
        isCurrentCollapsed = isIt;
    }

    bool isCurrentlyCollapsed() {
        return isCurrentCollapsed;
    }

    bool isVisibleDlg() const { return isVisible(); }
    void setVisibleDlg(bool isVisible) { setVisible(isVisible); }
};
