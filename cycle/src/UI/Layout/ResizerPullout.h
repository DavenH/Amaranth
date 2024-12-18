#pragma once

#include <App/SingletonAccessor.h>
#include <UI/Widgets/PulloutComponent.h>
#include "JuceHeader.h"

class IconButton;

class ResizerPullout :
        public Button::Listener
    ,	public Component
    , 	public SingletonAccessor
{
public:
    explicit ResizerPullout(SingletonRepo* repo);

    void buttonClicked(Button* button) override;
    void resized() override;
    void updateHighlight(int windowSize);

private:
    std::unique_ptr<PulloutComponent> pullout;

    std::unique_ptr<IconButton> smallIcon;
    std::unique_ptr<IconButton> medIcon;
    std::unique_ptr<IconButton> fullIcon;
    std::unique_ptr<IconButton> keybIcon;

    JUCE_LEAK_DETECTOR(ResizerPullout)
};