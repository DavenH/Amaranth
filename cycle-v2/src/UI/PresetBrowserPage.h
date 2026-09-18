#pragma once

#include <JuceHeader.h>

#include <functional>
#include <vector>

namespace CycleV2 {

class PresetBrowserPage final : public juce::Component,
                                private juce::ListBoxModel,
                                private juce::TextEditor::Listener {
public:
    using OpenCallback = std::function<bool(const juce::File&)>;

    PresetBrowserPage(
            std::vector<juce::File> directories,
            OpenCallback openCallback,
            std::function<void()> browseCallback,
            std::function<void()> closeCallback);

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    int getNumRows() override;
    void paintListBoxItem(
            int row, juce::Graphics& graphics, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void returnKeyPressed(int row) override;
    void textEditorTextChanged(juce::TextEditor&) override;

    void refreshFilter();
    void openSelected();

    std::vector<juce::File> files;
    std::vector<int> visibleIndices;
    OpenCallback onOpen;
    std::function<void()> onBrowse;
    std::function<void()> onClose;
    juce::Label title;
    juce::TextEditor search;
    juce::ListBox list;
    juce::Label empty;
    juce::TextButton browse { "Browse files..." };
    juce::TextButton open { "Open preset" };
    juce::TextButton close { "Close" };
};

}
