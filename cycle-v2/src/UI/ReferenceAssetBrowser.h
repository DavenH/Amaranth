#pragma once

#include <JuceHeader.h>

#include "Reference/ReferenceLibrary.h"
#include "UI/SidebarLibraryToolbar.h"

namespace CycleV2 {

class StandaloneAudioEngine;

class ReferenceAssetBrowser final : public juce::Component,
                                    private juce::Timer {
public:
    ReferenceAssetBrowser(ReferenceLibrary& library, StandaloneAudioEngine& engine);
    ~ReferenceAssetBrowser() override;

    void refresh();
    std::function<void(const juce::String&)> onAdd;
    std::function<void()> onImport;
    std::function<void()> onShowTray;
    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation() const;
    void resized() override;

    static juce::String dragDescription(const juce::String& id);
    static juce::String referenceIdFromDrag(const juce::var& description);

private:
    class List;
    void timerCallback() override;
    void audition(const juce::String& id);
    void addSelected();

    ReferenceLibrary& library;
    StandaloneAudioEngine& engine;
    SidebarLibraryToolbar toolbar { "Search assets..." };
    juce::Viewport viewport;
    std::unique_ptr<List> list;
    bool lastAuditioning {};
    juce::String lastAuditionPath;
};

}
