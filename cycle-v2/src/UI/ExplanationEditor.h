#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

struct Node;

class ExplanationEditor final : public juce::Component {
public:
    using SaveCallback = std::function<bool(const juce::String&)>;

    ExplanationEditor(const juce::String& title, const juce::String& typeHelp,
            const juce::String& text, SaveCallback save);
    void focusText();
    void dismiss();
    void resized() override;
    void paint(juce::Graphics& graphics) override;
    bool keyPressed(const juce::KeyPress& key) override;
    juce::var automationState() const;

private:
    void submit();

    juce::Label heading;
    juce::Label help;
    juce::TextEditor text;
    juce::TextButton save { "Save" };
    juce::TextButton cancel { "Cancel" };
    SaveCallback onSave;
};

class ExplanationBadge {
public:
    static constexpr int presetButtonWidth = 94;
    static bool isVisible(const Node& node, const juce::String& selectedNodeId);
    static juce::Rectangle<float> boundsFor(const Node& node,
            juce::Rectangle<float> nodeBounds, float zoom, bool globalProcessing);
    static float titleReservation(const Node& node, bool globalProcessing);
    static void paint(juce::Graphics& graphics, juce::Rectangle<float> bounds, bool authored);
};

}
