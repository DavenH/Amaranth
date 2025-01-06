#pragma once
#include <string>

#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/IConsole.h>
#include "JuceHeader.h"

using std::string;

class IconButton;
class ResizerPullout;


class Console:
        public juce::Component
    ,	public Timer
    ,	public IConsole {
public:
    explicit Console(SingletonRepo* repo);

    void timerCallback() override;
    void paint(Graphics& g) override;
    void setKeys(const String& keys) override;
    void write(const String& str, int priority) override;
    void setMouseUsage(const MouseUsage& usage) override;
    void setMouseUsage(bool left, bool scroll, bool middle, bool right) override;

    void init() override;
    void reset() override;
    void resized() override;
    void addPullout();

private:
    bool 	firstCallback;
    bool 	fading;
    int 	currentPriority;
    float 	opacity;

    String 	text;
    String 	shortcut;
    String 	keys{};
    Image 	mouseparts;
    MouseUsage usage;

    Ref<ResizerPullout> pullout;
};
