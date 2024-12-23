#pragma once

#include <utility>
#include <vector>
#include <iostream>
#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/MouseEventDelegator.h>

using std::cout;
using std::endl;
using std::vector;

class IconButton;
class MouseListenerCallback;

class HoverSelector  :
        public Component
    , 	public SingletonAccessor {
public:
    HoverSelector(SingletonRepo* repo, int x, int y, bool horz);

    ~HoverSelector() override = default;
    bool menuActive;

    void resized() override;

    void mouseEnter(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e) override;

    void showPopup();
    void setSelectedId(int id);
    void revertMesh();
    void paintOverChildren(Graphics& g) override;

    virtual void mouseOverItem(int itemIndex);
    virtual void mouseLeftItem(int itemIndex);
    virtual void populateMenu() = 0;
    virtual void itemWasSelected(int id) = 0;
    virtual bool itemIsSelection(int id) = 0;
    virtual void revert() {}
    virtual void prepareForPopup() {}

    PopupMenu menu;
    bool horizontal;

private:

    std::unique_ptr<IconButton> headerIcon;
};

class SelectorCallback :
        public Component
    ,	public MouseEventDelegatee
{
private:
    int fileIndex{}, itemIndex;

    String filename;
    Ref<HoverSelector> selector;
    MouseEventDelegator delegator;

    JUCE_LEAK_DETECTOR(SelectorCallback)

public: 
    SelectorCallback(int _itemIndex, String  _filename, HoverSelector* _selector) :
            itemIndex(_itemIndex)
        ,	filename(std::move(_filename))
        ,	selector(_selector)
        ,	delegator(this, this) {
        setMouseCursor(MouseCursor::PointingHandCursor);
    }

    ~SelectorCallback() override = default;

    void paint(Graphics& g) override {
        if (isMouseOver()) {
            g.fillAll(Colours::black.withAlpha(0.7f));
        }

        g.setColour(Colour::greyLevel(isMouseOver() ? 0.8f : 0.65f));
        g.setFont(16);

        String name = filename.substring(filename.lastIndexOf(File::getSeparatorString()) + 1, filename.lastIndexOf("."));
        g.drawText(name, 30, 0, 3 * getWidth() / 4, getHeight(), Justification::centredLeft, true);
    }

    const String& getFilename() {
        return filename;
    }

    void mouseEnter(const MouseEvent& e) override {
      #ifndef JUCE_MAC
//		if(e.originalComponent == this)
        enterDlg();
      #endif
    }

    void mouseExit(const MouseEvent& e) override {
      #ifndef JUCE_MAC
//		if(e.originalComponent == this)
        exitDlg();
      #endif
    }

    void enterDlg() override {
        selector->mouseOverItem(itemIndex);
        repaint();
    }

    void exitDlg() override {
        selector->mouseLeftItem(itemIndex);
        repaint();
    }
};