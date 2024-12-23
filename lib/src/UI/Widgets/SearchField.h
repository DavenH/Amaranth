#pragma once

#include <utility>
#include <vector>
#include "../IConsole.h"
#include "../MiscGraphics.h"
#include "../../App/SingletonRepo.h"
#include "../../Binary/Images.h"
#include "../../Obj/Ref.h"
#include "../../Util/FilterableList.h"
#include "../../Definitions.h"

using std::vector;

template<class T> class FilterableList;

template<class T>
class SearchField:
        public Component
    ,	public MultiTimer
    ,	public SingletonAccessor {
public:
    enum { BlinkTimerId, RefreshTimerId };

    SearchField(FilterableList<T>* list, SingletonRepo* repo, String defaultString) :
            SingletonAccessor(repo, "SearchField")
        ,	blink		  (false)
        ,	focused		  (false)
        ,	selectingAll  (false)
        ,	filterableList(list)
        ,	defaultString (std::move(defaultString))
        ,	caretPosition (0)
        ,	allowableChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ -_") {
        startTimer(BlinkTimerId, 400);
        setWantsKeyboardFocus(true);

        searchIcon = PNGImageFormat::loadFrom(Images::mag_png, Images::mag_pngSize);
        setMouseCursor(MouseCursor::IBeamCursor);
    }

    ~SearchField() override = default;

    void setText(const String& text) {
        searchString = text;
        String token;
        StringArray list;

        if (text.contains(" "))
            list.addTokens(searchString, " ", String());
        else
            list.add(text);

        filterableList->filterList(list);

        repaint();
    }

    void refresh() {
        setText(searchString);
    }

    bool keyPressed(const KeyPress& k) override {
        if (!focused) {
            return false;
        }

        stopTimer(RefreshTimerId);

        int code = k.getKeyCode();
        juce_wchar textChar = k.getTextCharacter();

        if (textChar >= '0' && textChar <= '9') {
            listener->keyPressed(k, this);
            return true;
        }

        if (code == KeyPress::backspaceKey || code == KeyPress::deleteKey) {
            if (searchString.length() > 0) {
                if (selectingAll) {
                    searchString = String();
                    selectingAll = false;
                    caretPosition = 0;
                } else {
                    if (code == KeyPress::deleteKey) {
                        searchString = searchString.substring(0, caretPosition) + searchString.substring(caretPosition + 1);
                    } else if (code == KeyPress::backspaceKey) {
                        if (ModifierKeys::getCurrentModifiers().isCommandDown()) {
                            searchString = String();
                            caretPosition = 0;
                        } else {
                            searchString = searchString.substring(0, caretPosition - 1) + searchString.substring(caretPosition);
                            caretPosition = jmax(0, caretPosition - 1);
                        }
                    }
                }

                startTimer(RefreshTimerId, 60);
            }
        } else if (code == KeyPress::returnKey) {
            filterableList->triggerLoadItem();
        } else if (code == KeyPress::leftKey) {
            caretPosition = jmax(0, caretPosition - 1);
            blink = false;
        } else if (code == KeyPress::rightKey) {
            caretPosition = jmin(searchString.length() - 1, caretPosition + 1);
            blink = false;
        } else if (textChar == 0) {
            return true;
        } else if (allowableChars.containsChar(textChar)) {
            if (selectingAll) {
                searchString = String();
                selectingAll = false;
            }

            if (caretPosition == searchString.length()) {
                searchString += k.getTextCharacter();
            } else {
                String newString = searchString.substring(0, caretPosition);
                newString << textChar << searchString.substring(caretPosition);
                searchString = newString;
            }

            caretPosition++;

            startTimer(RefreshTimerId, 60);
        }

        repaint();

        return true;
    }

    void paint(Graphics& g) override {
        Font sansSerif(FontOptions(Font::getDefaultSansSerifFontName(), 15, Font::plain));
        g.setFont(sansSerif);

        Rectangle<float> r(1, 1, getWidth() - 1, getHeight() - 1);

        g.setColour(Colour::greyLevel(0.08f));
        g.fillRoundedRectangle(r, 1.f);
        g.setColour(focused ? Colours::orange : Colour::greyLevel(0.25f));
        getObj(MiscGraphics).drawRoundedRectangle(g, r, 1.f);

        int strWidth = Util::getStringWidth(sansSerif, searchString);

        if (selectingAll) {
            g.setColour(Colour(180, 190, 240));
            g.fillRect(8, 3, strWidth, getHeight() - 6);
        }

        g.drawImageWithin(searchIcon, 0, 2, getWidth() - 2, getHeight() - 4,
                RectanglePlacement::yMid | RectanglePlacement::xRight, false);
        g.setOpacity(1.f);
        g.setColour(selectingAll ? Colours::grey : Colours::lightgrey);

        if (searchString.length() > 0 || focused) {
            if (strWidth > getWidth() - 5) {
                g.drawSingleLineText(searchString, 8 - (strWidth - getWidth()), getHeight() / 2 + 4);
            } else {
                g.drawSingleLineText(searchString, 8, getHeight() / 2 + 4);
            }
        }

        g.setColour(Colours::lightgrey);

        if (focused && blink) {
            int subWidth = Util::getStringWidth(sansSerif, searchString.substring(0, caretPosition));
            g.drawVerticalLine(subWidth + 8, 3, getHeight() - 3);
        }
    }

    void focusGained(FocusChangeType cause) override {
        focused = true;

        repaint();
    }

    void focusLost(FocusChangeType cause) override {
        focused = false;
        selectingAll = false;

        repaint();
    }

    void timerCallback(int id) override {
        switch (id) {
            case BlinkTimerId:
                if (!selectingAll) {
                    blink ^= 1;
                    repaint();
                }
                break;

            case RefreshTimerId:
                setText(searchString);
                filterableList->doChangeUpdate();
                stopTimer(RefreshTimerId);

                break;
            default: break;
        }
    }

    void mouseDoubleClick(const MouseEvent& e) override {
        selectingAll = true;
        blink = false;

        repaint();
    }

    void mouseEnter(const MouseEvent& e) override {
        repo->getConsole().updateAll("*", "Multi-keyword search all fields & tags",
                                     MouseUsage(true, false, true, true));
    }

    void mouseDown(const MouseEvent& e) override {
        setWantsKeyboardFocus(true);
        grabKeyboardFocus();

        if (searchString.length() > 0) {
            selectingAll ^= 1;
        }

        blink = ! selectingAll;

        repaint();
    }

private:
    bool focused;
    bool blink;
    bool selectingAll;

    int caretPosition;

    String searchString;
    String defaultString;
    String allowableChars;

    Ref<KeyListener> listener;
    Ref<FilterableList<T> > filterableList;

    Image searchIcon;

    JUCE_LEAK_DETECTOR(SearchField)
};
