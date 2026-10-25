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
class ReferencePitchEditor;

class ComparisonDock : public juce::Component,
                       public juce::DragAndDropTarget {
public:
    ComparisonDock(
            NodeCanvas& canvas,
            ReferenceLibrary& library,
            StandaloneAudioEngine& audioEngine);
    ~ComparisonDock() override;

    std::function<void(const juce::String&)> onReferenceDropped;
    bool isInterestedInDragSource(const SourceDetails& source) override;
    void itemDropped(const SourceDetails& source) override;
    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            pointerTargetsForAutomation() const;
    void refreshCaptures();
    void syncAuditionState();
    void importReference();
    bool openPitchEditorForAutomation(const juce::String& id);
    std::vector<std::pair<juce::String, juce::Rectangle<float>>>
            referenceEditorTargetsForAutomation() const;
    void renderOpenGL(float scaleFactor);
    void releaseOpenGLResources();
    juce::var exportAutomationState() const;
    void setHeightChangeCallback(std::function<void(int)> callback) {
        heightChangeCallback = std::move(callback);
    }
    void setReferenceImportedCallback(std::function<void()> callback) {
        referenceImportedCallback = std::move(callback);
    }
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    struct Card {
        juce::String id;
        juce::String title;
        std::vector<float> surface;
        const std::vector<float>* sampleSurface {};
        const PitchedSample* sampleIdentity {};
        uint64_t surfaceRevision {};
        size_t columns {};
        size_t rows {};
        PortDomain domain { PortDomain::TimeSignal };
        int midiNote {};
        float displayGain { 1.f };
        bool sample {};
        bool connected {};
        bool spectral {};

        const std::vector<float>& surfaceValues() const {
            return sampleSurface != nullptr ? *sampleSurface : surface;
        }
    };

    class CardStrip;

    void showAddMenu();
    void showCardMenu(const Card& card, juce::Point<int> position);
    void openPitchEditor(const juce::String& id);
    void closePitchEditor();
    void toggleAudition();
    void selectReference(const juce::String& id);
    void closeReference(const juce::String& id);
    void updateCards();
    void paintCard(juce::Graphics& graphics, const Card& card, juce::Rectangle<float> bounds);
    static juce::PopupMenu::Options menuOptionsFor(juce::Component& target);

    NodeCanvas& canvas;
    ReferenceLibrary& library;
    StandaloneAudioEngine& audioEngine;
    class AddButton;
    std::unique_ptr<AddButton> addButton;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<Card> cards;
    std::vector<std::unique_ptr<ComparisonSurfaceView>> surfaceViews;
    std::vector<std::unique_ptr<ComparisonSurfaceView>> retiredViews;
    std::unique_ptr<CardStrip> strip;
    std::unique_ptr<ReferencePitchEditor> pitchEditor;
    juce::Viewport viewport;
    std::function<void(int)> heightChangeCallback;
    std::function<void()> referenceImportedCallback;
    int resizeStartHeight {};
    bool resizing {};
    uint64_t captureCount {};
    bool lastAuditioning {};
    juce::String lastAuditionPath;
    juce::CriticalSection surfaceLock;
};

}
