#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class LibrarySearchField final : public juce::TextEditor,
                                private juce::KeyListener {
public:
    explicit LibrarySearchField(const juce::String& placeholder);

    void setPlaybackToggleCallback(std::function<void()> callback);
    bool handlePlaybackSpace(const juce::KeyPress& key);
    bool keyPressed(const juce::KeyPress& key) override;
    void insertTextAtCaret(const juce::String& text) override;
    void paintOverChildren(juce::Graphics& graphics) override;

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;
    std::function<void()> onPlaybackToggle;
};

}
