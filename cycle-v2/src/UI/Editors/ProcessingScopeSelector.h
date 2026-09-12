#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class ProcessingScopeSelector final : public juce::Component {
public:
    explicit ProcessingScopeSelector(juce::String componentIdPrefix);
    ~ProcessingScopeSelector() override;

    void setScope(const juce::String& scope, juce::NotificationType notification);
    juce::String scope() const;
    juce::var automationState() const;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

    std::function<void(const juce::String&)> onChange;

private:
    class ScopeButton;

    void focusSegment(int index);
    void selectSegment(int index, juce::NotificationType notification);

    std::unique_ptr<ScopeButton> voice;
    std::unique_ptr<ScopeButton> global;
    int selectedSegment {};
};

}
