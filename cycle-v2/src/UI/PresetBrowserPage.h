#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "UI/PresetBrowserComponents.h"

namespace CycleV2 {

class PresetBrowserPage final : public juce::Component,
                                private juce::TextEditor::Listener,
                                private juce::KeyListener {
public:
    using OpenCallback = std::function<bool(const juce::File&)>;

    PresetBrowserPage(
            std::vector<juce::File> directories,
            OpenCallback openCallback,
            std::function<void()> browseCallback,
            std::function<void()> closeCallback);

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;
    void textEditorTextChanged(juce::TextEditor&) override;
    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void receiveResults(
            const std::vector<PresetLibraryRecord>& records,
            const std::vector<int>& visibleIndices);
    void updateSelection();
    void openSelected();

    OpenCallback onOpen;
    std::function<void()> onBrowse;
    std::function<void()> onClose;
    juce::Label title;
    juce::Label subtitle;
    juce::TextEditor search;
    PresetBrowserSidebar sidebar;
    juce::Viewport viewport;
    PresetCardGrid grid;
    PresetDetailPanel detail;
    juce::Label status;
    juce::TextButton browse { "BROWSE FILES" };
    juce::TextButton open { "LOAD PRESET" };
    juce::TextButton close { "CLOSE" };
    std::unique_ptr<PresetLibraryIndex> index;
};

}
