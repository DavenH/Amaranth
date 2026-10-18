#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "Reference/ReferenceLibrary.h"
#include "UI/NodeCanvas.h"

namespace CycleV2 {

class StandaloneAudioEngine;
class ComparisonSurfaceView;

class ComparisonDock : public juce::Component {
public:
    ComparisonDock(
            NodeCanvas& canvas,
            ReferenceLibrary& library,
            StandaloneAudioEngine& audioEngine);
    ~ComparisonDock() override;

    void refreshCaptures();
    void syncAuditionState();
    void renderOpenGL(float scaleFactor);
    void releaseOpenGLResources();
    juce::var exportAutomationState() const;
    void setHeightChangeCallback(std::function<void(int)> callback) {
        heightChangeCallback = std::move(callback);
    }
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    struct Card {
        juce::String id;
        juce::String title;
        juce::String detail;
        std::vector<float> surface;
        const std::vector<float>* sampleSurface {};
        const PitchedSample* sampleIdentity {};
        uint64_t surfaceRevision {};
        size_t columns {};
        size_t rows {};
        PortDomain domain { PortDomain::TimeSignal };
        int midiNote {};
        bool sample {};
        bool connected {};
        bool spectral {};

        const std::vector<float>& surfaceValues() const {
            return sampleSurface != nullptr ? *sampleSurface : surface;
        }
    };

    class CardStrip;

    void importReference();
    void showReferenceMenu();
    void showSpyMenu();
    void selectReference(const juce::String& id);
    void updateCards();
    void updateNoteControl();
    void paintCard(juce::Graphics& graphics, const Card& card, juce::Rectangle<float> bounds);
    static juce::Rectangle<float> pitchBounds(juce::Rectangle<float> cardBounds);
    void commitPitchPoint(size_t pointIndex, juce::Point<float> position);
    static juce::PopupMenu::Options menuOptionsFor(juce::Component& target);

    NodeCanvas& canvas;
    ReferenceLibrary& library;
    StandaloneAudioEngine& audioEngine;
    juce::TextButton importButton { "Import Reference" };
    juce::TextButton referenceButton { "Reference" };
    juce::TextButton spyButton { "Add Spy" };
    juce::TextButton playButton { "Play" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::Slider noteSlider;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<Card> cards;
    std::vector<std::unique_ptr<ComparisonSurfaceView>> surfaceViews;
    std::vector<std::unique_ptr<ComparisonSurfaceView>> retiredViews;
    std::unique_ptr<CardStrip> strip;
    juce::Viewport viewport;
    std::function<void(int)> heightChangeCallback;
    int resizeStartHeight {};
    bool resizing {};
    uint64_t captureCount {};
    juce::CriticalSection surfaceLock;
};

}
