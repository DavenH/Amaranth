#pragma once

#include <JuceHeader.h>

class Interactor;
class Panel;

class PanelInputHostComponent : public juce::Component {
public:
    explicit PanelInputHostComponent(Panel& panel);

protected:
    Interactor* panelInteractor() const;

    virtual bool acceptsPointerDown(const juce::MouseEvent& event) const;
    virtual bool acceptsDoubleClick(const juce::MouseEvent& event) const;
    virtual void pointerGestureBegan() {}
    virtual void pointerGestureUpdated() {}
    virtual void pointerGestureEnded() {}
    virtual bool deleteKeyPressed() { return false; }

private:
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseWheelMove(
            const juce::MouseEvent& event,
            const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void resized() override;

    Panel& panel;
    bool pointerActive {};
};
