#pragma once

#include "IDynamicSizeComponent.h"
#include "JuceHeader.h"
using namespace juce;

class ConstantSizeComponent :
        public IDynamicSizeComponent,
        public juce::Component {
    bool isCurrentCollapsed;
    int size;

public:
    explicit ConstantSizeComponent(int size)
        : isCurrentCollapsed(false), size(size) {
    }

    [[nodiscard]] int getExpandedSize() const override {
        return size;
    }

    [[nodiscard]] int getCollapsedSize() const override {
        return getExpandedSize();
    }

    void setBoundsDelegate(int x, int y, int w, int h) override {
        setBounds(x, y, w, h);
    }

    [[nodiscard]] Rectangle<int> getBoundsInParentDelegate() const override {
        return getBoundsInParent();
    }

    int getYDelegate() override {
        return getY();
    }

    int getXDelegate() override {
        return getX();
    }

    void setCurrentlyCollapsed(bool isIt) override {
        isCurrentCollapsed = isIt;
    }

    bool isCurrentlyCollapsed() override {
        return isCurrentCollapsed;
    }

    [[nodiscard]] bool isVisibleDlg() const override { return isVisible(); }
    void setVisibleDlg(bool isVisible) override { setVisible(isVisible); }
};
