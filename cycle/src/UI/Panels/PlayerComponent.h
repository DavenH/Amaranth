#pragma once

#include <Obj/Ref.h>
#include <UI/Widgets/IconButton.h>
#include <App/SingletonAccessor.h>

#include "../Widgets/MidiKeyboard.h"

class ResizerPullout;
class PulloutComponent;
class RetractableCallout;

class PlayerComponent :
        public juce::Component
    ,	public Button::Listener
    ,	public SingletonAccessor
{
public:
    explicit PlayerComponent(SingletonRepo* repo);
    void init() override;
    void setComponents(bool add);
    void buttonClicked(Button* button) override;
    void paint(Graphics& g) override;
    void resized() override;

    void updateTitle();

private:
    IconButton 			prevIcon;
    IconButton 			nextIcon;
    IconButton 			tableIcon;

    Ref<MidiKeyboard> 	keyboard;
    Ref<ResizerPullout> pullout;

    String				name;
    String				author;

    JUCE_LEAK_DETECTOR(PlayerComponent)
};