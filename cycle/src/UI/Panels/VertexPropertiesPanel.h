#pragma once

#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/MouseOverMessager.h>
#include <UI/Widgets/InsetLabel.h>
#include "JuceHeader.h"
#include <set>
#include <vector>
#include <UI/TourGuide.h>

using std::vector;
using std::set;

class Panel;
class TwoTextButton;
class Interactor;
class Knob;
class HSlider;
class Interactor;
class TwoStateButton;
class LabelHint;

class VertexPropertiesPanel :
        public juce::Component
    ,	public Slider::Listener
    ,	public Button::Listener
    ,	public ComboBox::Listener
    ,	public TourGuide
    ,	public AsyncUpdater
    ,	public SingletonAccessor
{
public:
    explicit VertexPropertiesPanel(SingletonRepo* repo);
    ~VertexPropertiesPanel() override;

    Component* getComponent(int which) override;
    void buttonClicked(Button* button) override;
    void comboBoxChanged (ComboBox* comboBoxThatHasChanged) override;
    void drawJustifiedText(Graphics& g, String text, Component* component, int y);
    void drawSidewaysTextAndLines(Graphics& g, String text, Component* top, Component* bottom);
    void gainChanged(Slider* slider, int changeType);
    void getSourceDestDimensionIds(int id, int& srcId, int& destId);
    void handleAsyncUpdate() override;
    void init() override;
    void mouseEnter(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void paint(Graphics & g) override;
    void refreshCube(set<TrilinearCube*>& lines);
    void resized() override;
    void setSelectedAndCaller(Interactor* interactor);
    void sliderDragEnded(Slider* slider) override;
    void sliderDragStarted(Slider* slider) override;
    void sliderValueChanged(Slider* slider) override;
    void triggerSliderChange(int dim, float value);
    void updateComboBoxes();
    void updateSliderProperties();
    void updateSliderValues(bool ignoreChangeMessage);

    [[nodiscard]] Interactor* getCurrentInteractor() const { return currentInteractor; }

private:
    void refreshValueBoxesFromSelected();
    Interactor* currentInteractor;

    Ref<Font> goldfish;
    Ref<Font> silkscreen;

    Label titleLabel;
    Label titleLabelA;

    enum SliderOrder { TimeSldr, KeySldr, ModSldr, PhaseSldr, AmpSldr, CurveSldr, numSliders };
    enum GuideIds { NullPathId = 1, /* NewPathId */ };
    enum { ValueChanged, DragEnded, DragStarted };


    class GainListener : public Slider::Listener
    {
    public:
        explicit GainListener(VertexPropertiesPanel* panel) : panel(panel) {}
        void sliderValueChanged(Slider* slider) override { panel->gainChanged(slider, ValueChanged); }
        void sliderDragEnded(Slider* slider) override	 { panel->gainChanged(slider, DragEnded); 	}
        void sliderDragStarted(Slider* slider) override	 { panel->gainChanged(slider, DragStarted); 	}

    private:
        Ref<VertexPropertiesPanel> panel;
    };

    class VertexProperties
    {
    public:
        VertexProperties(VertexPropertiesPanel* panel, const String& name, const String& text, int id);

        virtual ~VertexProperties();
        void setValueToCurrent(bool sendChangeMessage);
        void setGainValueToCurrent(bool sendChangeMessage);
        VertexProperties& operator=(const VertexProperties& copy);
        VertexProperties(const VertexProperties& copy);

        int id;
        float previousValue;
        float previousGain;

        HSlider* 			slider;
        ComboBox* 			pathChanBox;
        Knob*				gain;
        MouseOverMessager* 	messager;
    };

    GlowEffect glow;
    GainListener gainListener;
    String ampVsPhaseStr;

    Component sliderArea, boxArea, gainArea;

    vector<VertexProperties> properties;
    vector<VertexProperties*> allProperties;
    vector<VertexProperties*> gainProperties;

    std::unique_ptr<VertexProperties> ampVsPhaseProperties;

    InsetLabel title;
};
