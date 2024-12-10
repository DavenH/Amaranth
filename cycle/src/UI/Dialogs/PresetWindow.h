#ifndef PRESETWINDOW_H_
#define PRESETWINDOW_H_

#include "JuceHeader.h"

class SingletonRepo;

class PresetWindow : public DialogWindow
{
public:
    PresetWindow ();
    void show(SingletonRepo* repo, Component* toCentreAround);
    void closeButtonPressed();

private:
    JUCE_DECLARE_NON_COPYABLE (PresetWindow);
};

#endif
