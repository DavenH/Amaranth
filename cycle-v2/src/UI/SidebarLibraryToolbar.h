#pragma once

#include <JuceHeader.h>

#include <vector>

#include "UI/LibrarySearchField.h"

namespace CycleV2 {

class SidebarLibraryToolbar final : public juce::Component {
public:
    static constexpr int height = 79;

    explicit SidebarLibraryToolbar(const juce::String& searchPlaceholder);

    LibrarySearchField& searchField() { return search; }
    juce::TextButton& newButton() { return create; }
    juce::TextButton& editButton() { return edit; }
    juce::TextButton& renameButton() { return rename; }
    juce::TextButton& deleteButton() { return remove; }

    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation() const;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    LibrarySearchField search;
    juce::TextButton create { "+ NEW" };
    juce::TextButton edit { "EDIT" };
    juce::TextButton rename { "RENAME" };
    juce::TextButton remove { "DELETE" };
};

}
