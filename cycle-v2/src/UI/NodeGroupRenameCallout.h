#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class NodeGroupRenameCallout final :
        public juce::Component
    ,   public juce::LookAndFeel_V4 {
public:
    using RenameCallback = std::function<void(const juce::String&)>;

    NodeGroupRenameCallout(const juce::String& currentName, RenameCallback rename);

    void focusName();
    void resized() override;
    void drawCallOutBoxBackground(
            juce::CallOutBox& box,
            juce::Graphics& graphics,
            const juce::Path& outline,
            juce::Image& cachedImage) override;
    int getCallOutBoxBorderSize(const juce::CallOutBox& box) override;
    float getCallOutBoxCornerSize(const juce::CallOutBox& box) override;

private:
    void submit();
    void dismiss();

    juce::TextEditor name;
    RenameCallback onRename;
};

}
