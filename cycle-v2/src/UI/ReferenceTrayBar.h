#pragma once

#include <JuceHeader.h>

#include "UI/PresetBrowserLookAndFeel.h"

namespace CycleV2 {

class ReferenceTrayBar final : public juce::Component,
                               public juce::DragAndDropTarget {
public:
    ReferenceTrayBar();
    ~ReferenceTrayBar() override;
    void setExpanded(bool expanded);
    std::function<void()> onToggle;
    std::function<void()> onBrowse;
    std::function<void()> onImport;
    std::function<void(const juce::String&)> onDrop;
    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation() const;
    bool isInterestedInDragSource(const SourceDetails& source) override;
    void itemDropped(const SourceDetails& source) override;
    void itemDragEnter(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    PresetBrowserLookAndFeel lookAndFeel;
    juce::TextButton disclosure;
    juce::TextButton browse { "ASSETS" };
    juce::TextButton import { "IMPORT..." };
    bool highlighted {};
    bool expanded {};
};

}
